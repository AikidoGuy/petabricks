/***************************************************************************
 *   Copyright (C) 2008 by Jason Ansel                                     *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "autotuner.h"
#include "hecuraruntime.h"

#include <limits>
#include <algorithm>


JTUNABLE(autotune_alg_slots,                5, 1, 32);
JTUNABLE(autotune_branch_attempts,          3, 1, 32);
JTUNABLE(autotune_improvement_threshold,    90, 10, 100);

#define MAX_ALGS autotune_alg_slots
#define BIRTH_ATTEMPTS autotune_branch_attempts
#define BIRTH_THRESH  (autotune_improvement_threshold.value()/100.0)
#define FIXED_CUTOFF

#ifdef FIXED_CUTOFF
static const int DUP_CUTOFF_THRESH = 1; // how different cutoffs must be to be duplicates
#else
static const int DUP_CUTOFF_THRESH = 1024; // how different cutoffs must be to be duplicates
#endif

namespace{ //file local 
  struct CmpLastPerformance {
    bool operator() (const hecura::CandidateAlgorithmPtr& a, const hecura::CandidateAlgorithmPtr& b){
      return a->lastResult() < b->lastResult();
    }
  };

  struct CmpLastLastPerformance {
    bool operator() (const hecura::CandidateAlgorithmPtr& a, const hecura::CandidateAlgorithmPtr& b){
      return a->lastlastResult() < b->lastlastResult();
    }
  };

  struct CmpAlgType {
    bool operator() (const hecura::CandidateAlgorithmPtr& a, const hecura::CandidateAlgorithmPtr& b){
      if(a->lvl() != b->lvl()) return a->lvl() < b->lvl();
      hecura::CandidateAlgorithmPtr ta=a, tb=b;
      while(ta && tb){
        if(ta->alg() != tb->alg()) return ta->alg() < tb->alg();
        ta=ta->next();
        tb=tb->next();
      }
      return a->cutoff() < b->cutoff();
    }
  };

  std::string _mktname(int lvl, const std::string& prefix, const std::string& type){
    return prefix + "_lvl" + jalib::XToString(lvl) + "_" + type;
  }
}

jalib::JTunable* hecura::Autotuner::algTunable(int lvl){
  return _tunableMap[_mktname(lvl, _prefix, "rule")];
}
jalib::JTunable* hecura::Autotuner::cutoffTunable(int lvl){
  return _tunableMap[_mktname(lvl, _prefix, "cutoff")];
}


hecura::Autotuner::Autotuner(HecuraRuntime& rt, std::string& prefix) 
  : _runtime(rt)
  , _tunableMap(jalib::JTunableManager::instance().getReverseMap())
  , _prefix(prefix)
{
  using jalib::JTunable;
  //find numlevels
  for(int lvl=2; true; ++lvl){
    JTunable* rule   = algTunable(lvl);
    JTunable* cutoff = cutoffTunable(lvl);
    if(rule==0 && cutoff==0){
      _maxLevels = lvl-1;
      break;
    }
  }
  JASSERT(_maxLevels>1)(prefix).Text("invalid prefix to autotune");

  //make initialconfig (all level disabled)
  for(int lvl=1; lvl<=_maxLevels; ++lvl){
    JTunable* at   = algTunable(lvl);
    JTunable* ct = cutoffTunable(lvl);
    int a=0, c=std::numeric_limits<int>::max();
    if(ct!=0) c=ct->max();
    _initialConfig = new CandidateAlgorithm(lvl, a, at, c, ct, _initialConfig);
  }

  //add 1 level candidates
  CandidateAlgorithmList lvl1Candidates;
  {
    JTunable* at = algTunable(1);
    JTunable* ct = cutoffTunable(1);
    int a=0, c=1;
    if(at==0){
      lvl1Candidates.push_back(new CandidateAlgorithm(1, a, at, c, ct, NULL));
    }else{
      for(a=at->min(); a<=at->max(); ++a){
        lvl1Candidates.push_back(new CandidateAlgorithm(1, a, at, c, ct, NULL));
      }
    }
  }

  //add 2 level candidates
  CandidateAlgorithmList lvl2Candidates;
  {
    JTunable* at = algTunable(2);
    JTunable* ct = cutoffTunable(2);
    int a=0, c=1;
    if(at==0){
      for(CandidateAlgorithmList::const_iterator i=lvl1Candidates.begin(); i!=lvl1Candidates.end(); ++i)
        lvl2Candidates.push_back(new CandidateAlgorithm(2, a, at, c, ct, *i));
    }else{
      for(a=at->min(); a<=at->max(); ++a){
        for(CandidateAlgorithmList::const_iterator i=lvl1Candidates.begin(); i!=lvl1Candidates.end(); ++i)
          lvl2Candidates.push_back(new CandidateAlgorithm(2, a, at, c, ct, *i));
      }
    }
  }

  JTRACE("Autotuner constructed")(lvl1Candidates.size())(lvl2Candidates.size());
  _candidates.swap(lvl1Candidates);
  _candidates.insert(_candidates.end(), lvl2Candidates.begin(), lvl2Candidates.end());
}


void hecura::Autotuner::runAll(){
  for(CandidateAlgorithmList::iterator i=_candidates.begin(); i!=_candidates.end(); ++i){
    _initialConfig->activate();
    (*i)->activate();
    double d = _runtime.runTrial();
    (*i)->addResult(d);
    fflush(stdout);
  }
}

void hecura::Autotuner::train(int min, int max){
  for(int n=min; n<=max; n*=2){
    _runtime.setSize(n);
    trainOnce();
  }
}

void hecura::Autotuner::trainOnce(){
  std::cout << "BEGIN ITERATION " << _prefix << " / " << _runtime.curSize() << std::endl;
  runAll();

  std::sort(_candidates.begin(), _candidates.end(), CmpLastPerformance());
  double bestPerf = _candidates[0]->lastResult();

  // add new algorithms -- by last rounds performance
  std::sort(_candidates.begin(), _candidates.end(), CmpLastLastPerformance());
  for(int i=0; i<BIRTH_ATTEMPTS && i<_candidates.size(); ++i){
    _initialConfig->activate();
    CandidateAlgorithmPtr b=_candidates[i]->attemptBirth(_runtime, *this, bestPerf*BIRTH_THRESH);
    if(b){
      _candidates.push_back(b);
      std::cout << "  ADDED " << b << std::endl;
    }
  }
  
  removeDuplicates();
  
  std::sort(_candidates.begin(), _candidates.end(), CmpLastPerformance());
  //kill slowest algorithms
  for(int i=_candidates.size()-1; i>=MAX_ALGS; --i){
      std::cout << "  REMOVED " << _candidates[i] << std::endl;
      _candidates.pop_back();
  }

  printCanidates();

  //reset config
  _initialConfig->activate();
  _candidates[0]->activate();
}

void hecura::Autotuner::printCanidates(){
  for(CandidateAlgorithmList::iterator i=_candidates.begin(); i!=_candidates.end(); ++i){
    std::cout << "  * " << jalib::StringPad((*i)->toString(),20) << " = " << (*i)->lastResult() << std::endl; 
  }
}

void hecura::Autotuner::removeDuplicates(){
  std::sort(_candidates.begin(), _candidates.end(), CmpAlgType());
  //kill duplicates
  for(int i=0; i<_candidates.size()-1; ++i){
    if(_candidates[i]->isDuplicate(_candidates[i+1])){
      if(_candidates[i]->lastResult() > _candidates[i+1]->lastResult()){
        std::cout << "  DUPLICATE " << _candidates[i] << std::endl;
        _candidates.erase(_candidates.begin()+i);
      }else{
        std::cout << "  DUPLICATE " << _candidates[i+1] << std::endl;
        _candidates.erase(_candidates.begin()+i+1);
      }
      --i; //redo this iteration
    }
  }
}

hecura::CandidateAlgorithmPtr hecura::CandidateAlgorithm::attemptBirth(HecuraRuntime& rt, Autotuner& autotuner, double thresh) {
  CandidateAlgorithmList possible;
  activate();

#ifndef FIXED_CUTOFF
  if(_cutoffTunable!=0){
    int min = _cutoffTunable->min();
    if(_nextLevel) min=_nextLevel->cutoff();
    int max = std::min(rt.curSize(), _cutoffTunable->max());
    if(max>500) max = std::min(max, _cutoff*4);
    double p = rt.optimizeParameter(*_cutoffTunable, min, max);
    if(p<thresh && p>=0){
      CandidateAlgorithmPtr c = new CandidateAlgorithm(_lvl, _alg, _algTunable, _cutoffTunable->value() , _cutoffTunable, _nextLevel);
      c->addResult(p);
      if(c->isDuplicate(this)){
        _cutoff=_cutoffTunable->value();
        addResult(p);
      }else
        return c;
    }
    activate();
  }
#endif 

  jalib::JTunable* at = autotuner.algTunable(_lvl+1);
  jalib::JTunable* ct = autotuner.cutoffTunable(_lvl+1);
  if(ct!=0){
    int min = _cutoff;
    int max = std::min(rt.curSize(), ct->max());
    int amin=0,amax=0;
    if(at!=0){
      amin=at->min();
      amax=at->max();
    }
    for(int a=amin; a<=amax; ++a){
      if(_lvl>1 && a==_alg) continue;
      if(at!=0) at->setValue(a);
#ifndef FIXED_CUTOFF
      double p = rt.optimizeParameter(*ct, min, max); 
#else
      ct->setValue(rt.curSize() * 3 / 4);
      if (ct->value() <= 1) {
        continue;
      }
      double p = rt.runTrial();
#endif
      if(p<thresh && p>=0){
        CandidateAlgorithmPtr c = new CandidateAlgorithm(_lvl+1, a, at, ct->value(), ct, this);
        c->addResult(p);
        possible.push_back(c);
      }
    }
  }

  if(possible.empty())
    return 0;

  std::sort(possible.begin(), possible.end(), CmpLastPerformance());
  return possible[0];
}

bool hecura::CandidateAlgorithm::isDuplicate(const CandidateAlgorithmPtr& that){
  if(!that) return false;
  if(_lvl != that->lvl()) return false;
  if(_alg != that->alg()) return false;
  if(std::abs(_cutoff - that->cutoff()) > DUP_CUTOFF_THRESH) return false;
  if(_nextLevel) return _nextLevel->isDuplicate(that->next());
  return true;
}

