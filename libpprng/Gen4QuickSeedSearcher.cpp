/*
  Copyright (C) 2011-2012 chiizu
  chiizu.pprng@gmail.com
  
  This file is part of libpprng.
  
  libpprng is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  libpprng is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with libpprng.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "Gen4QuickSeedSearcher.h"
#include "SearchCriteria.h"

namespace pprng
{

namespace
{

struct SeedChecker
{
  SeedChecker(const Gen4QuickSeedSearcher::Criteria &criteria,
              const Gen4QuickSeedSearcher::ResultCallback &resultHandler)
    : m_criteria(criteria), m_resultHandler(resultHandler)
  {}
  
  void operator()(const Gen34Frame &frame) const
  {
    uint32_t  seed = frame.rngValue;
    uint32_t  frameNumber = 0, limit = m_criteria.frame.min - 1;
    
    while (frameNumber < limit)
    {
      seed = LCRNG34_R::NextForSeed(seed);
      ++frameNumber;
    }
    
    limit = m_criteria.frame.max;
    while (frameNumber < limit)
    {
      seed = LCRNG34_R::NextForSeed(seed);
      ++frameNumber;
      
      TimeSeed  ts(seed);
      
      if (ts.IsValid() &&
          (ts.BaseDelay() >= m_criteria.delay.min) &&
          (ts.BaseDelay() <= m_criteria.delay.max))
      {
        Gen34Frame  actualFrame = frame;
        actualFrame.seed = seed;
        actualFrame.number = frameNumber;
        
        Gen4Frame  g4Frame(actualFrame);
        
        Gen4Frame::EncounterData  *data;
        
        if ((m_criteria.version == Game::HeartGoldVersion) ||
            (m_criteria.version == Game::SoulSilverVersion))
        {
          data = &g4Frame.methodK;
        }
        else
        {
          data = &g4Frame.methodJ;
        }
        
        if ((data->landESVs & m_criteria.landESVs) ||
            (data->surfESVs & m_criteria.surfESVs) ||
            (data->oldRodESVs & m_criteria.oldRodESVs) ||
            (data->goodRodESVs & m_criteria.goodRodESVs) ||
            (data->superRodESVs & m_criteria.superRodESVs))
        {
          m_resultHandler(g4Frame);
        }
        
        break;
      }
    }
  }
  
  const Gen4QuickSeedSearcher::Criteria       &m_criteria;
  const Gen4QuickSeedSearcher::ResultCallback &m_resultHandler;
};

struct FrameChecker
{
  FrameChecker(const Gen4QuickSeedSearcher::Criteria &criteria)
    : m_criteria(criteria)
  {}
  
  bool operator()(const Gen34Frame &frame) const
  {
    return CheckShiny(frame.pid) && CheckNature(frame.pid) &&
           CheckAbility(frame.pid) && CheckGender(frame.pid) &&
           CheckIVs(frame.ivs) && CheckHiddenPower(frame.ivs);
  }
  
  bool CheckShiny(const PID &pid) const
  {
    return !m_criteria.shinyOnly || pid.IsShiny(m_criteria.tid, m_criteria.sid);
  }
  
  bool CheckNature(const PID &pid) const
  {
    return m_criteria.pid.CheckNature(pid.Gen34Nature());
  }
  
  bool CheckAbility(const PID &pid) const
  {
    return (m_criteria.pid.ability == Ability::ANY) ||
           (m_criteria.pid.ability == pid.Gen34Ability());
  }
  
  bool CheckGender(const PID &pid) const
  {
    return Gender::GenderValueMatches(pid.GenderValue(),
                                      m_criteria.pid.gender,
                                      m_criteria.pid.genderRatio);
  }
  
  bool CheckIVs(const IVs &ivs) const
  {
    return ivs.betterThanOrEqual(m_criteria.ivs.min) &&
           (m_criteria.ivs.max.isMax() ||
            ivs.worseThanOrEqual(m_criteria.ivs.max));
  }

  bool CheckHiddenPower(const IVs &ivs) const
  {
    return m_criteria.ivs.CheckHiddenPower(ivs.HiddenType(), ivs.HiddenPower());
  }
  
  const Gen4QuickSeedSearcher::Criteria  &m_criteria;
};

struct SeedSearcher
{
  typedef Gen34Frame  ResultType;
  
  void Search(uint32_t seed, const FrameChecker &frameChecker,
              const SeedChecker &resultHandler)
  {
    Method1FrameGenerator  frameGenerator(seed);
    
    frameGenerator.AdvanceFrame();
    Gen34Frame  result = frameGenerator.CurrentFrame();
    
    if (frameChecker(result))
      resultHandler(result);
  }
};

}

uint64_t Gen4QuickSeedSearcher::Criteria::ExpectedNumberOfResults()
{
  uint64_t  delays = delay.max - delay.min + 1;
  
  uint64_t  numSeeds = delays * 256 * 24;
  
  uint64_t  numFrames = frame.max - frame.min + 1;
  
  uint32_t  numIVs = IVs::CalculateNumberOfCombinations(ivs.min, ivs.max);
  
  uint64_t  natureMultiplier = pid.NumNatures(), natureDivisor = 25;
  
  uint64_t  shinyDivisor = shinyOnly ? 8196 : 1;
  
  uint64_t  numResults = numFrames * numSeeds * numIVs * natureMultiplier /
    (32 * 32 * 32 * 32 * 32 * 32 * natureDivisor * shinyDivisor);
  
  numResults = IVs::AdjustExpectedResultsForHiddenPower
  (numResults, ivs.min, ivs.max, ivs.hiddenTypeMask, ivs.minHiddenPower);
  
  return numResults;
}

void Gen4QuickSeedSearcher::Search
  (const Criteria &criteria, const ResultCallback &resultHandler,
   const SearchRunner::ProgressCallback &progressHandler)
{
  Gen34IVSeedGenerator  seedGenerator(criteria.ivs.min, criteria.ivs.max);
  SeedSearcher          seedSearcher;
  
  FrameChecker          frameChecker(criteria);
  SeedChecker           seedChecker(criteria, resultHandler);
  
  SearchRunner          searcher;
  
  searcher.Search(seedGenerator, seedSearcher, frameChecker,
                  seedChecker, progressHandler);
}

}
