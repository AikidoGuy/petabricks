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
#include "rulechoice.h"

#include "codegenerator.h"
#include "transform.h"
#include "rule.h"
#include "staticscheduler.h"

namespace {
  struct RuleIdComparer {
    bool operator()(const hecura::RulePtr& x, const hecura::RulePtr& y){
      return x->id() < y->id();
    }
  };

}

const hecura::FormulaPtr& hecura::RuleChoice::autotuned() {
  static FormulaPtr t = new FormulaVariable("AUTOTUNED");
  return t;
}

hecura::RuleChoice::RuleChoice( const RuleSet& rule
                              , const FormulaPtr& c/*=FormulaPtr()*/
                              , const RuleChoicePtr& n/*=RuleChoicePtr()*/)
  : _rules(rule), _condition(c), _next(n)
{}

void hecura::RuleChoice::print(std::ostream& o) const {
  o << "RuleChoice"; //TODO
}

void hecura::RuleChoice::generateCodeSimple(  const std::string& taskname
                                            , Transform& trans
                                            , ScheduleNode& node
                                            , const SimpleRegionPtr& region
                                            , CodeGenerator& o
                                            , const std::string& _tpfx)
{
  std::string tpfx = _tpfx;
  if(tpfx.length()==0) tpfx = trans.createTunerPrefix();

  if(_condition){
    o.beginIf(processCondition(tpfx + "lvl" + jalib::XToString(level()) + "_cutoff",_condition, o)->toString());
  }

  std::vector<RulePtr> sortedRules(_rules.begin(), _rules.end());
  std::sort(sortedRules.begin(), sortedRules.end(), RuleIdComparer());

  std::string choicename = tpfx + "lvl" + jalib::XToString(level()) + "_rule";
  int n=0;
  if(sortedRules.size()>1){
//     for(std::vector<RulePtr>::const_iterator i=sortedRules.begin(); i!=sortedRules.end(); ++i){
//       choicename += "_"+jalib::XToString((*i)->id() - trans.ruleIdOffset());
//     }
    o.createTunable(trans.name(), choicename, 0, 0, sortedRules.size()-1);
    o.beginSwitch(choicename);
  }
  for(std::vector<RulePtr>::const_iterator i=sortedRules.begin(); i!=sortedRules.end(); ++i){
    if(sortedRules.size()>1) o.beginCase(n++);
    if(taskname.empty())
      (*i)->generateCallCodeSimple(trans, o, region);
    else{
      (*i)->generateCallTaskCode(taskname, trans, o, region);
      node.printDepsAndEnqueue(o, sortedRules[0]);
    }
    if(sortedRules.size()>1) o.endCase();
  }
  if(sortedRules.size()>1){
    o.write("default: JASSERT(false)("+choicename+".value());");
    o.endSwitch();
  }

  if(_condition){
    if(_next){
      o.elseIf();
      _next->generateCodeSimple(taskname, trans, node, region, o, tpfx);
    }
    o.endIf();
  }
}

hecura::FormulaPtr hecura::RuleChoice::processCondition(const std::string& name, const FormulaPtr& f, CodeGenerator& o)
{
  if(f->getFreeVariables()->contains(autotuned()->toString())){
    o.createTunable(name, name, std::numeric_limits<int>::max(), 1);
    return f->replace(autotuned(), new FormulaVariable(name));
  }else{
    return f;
  }
}
