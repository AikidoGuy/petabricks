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
#include "choicegrid.h"
#include "rule.h"
#include "jconvert.h"
#include "maximawrapper.h"
#include "codegenerator.h"

hecura::ChoiceGridPtr hecura::ChoiceGrid::constructFrom( 
  const RuleSet&                allowedRules,
  const RuleDescriptorListList& dimensions,
  size_t                        dimension /*= 0*/)
{
  FormulaPtr    currentPos = FormulaInteger::zero();
  ChoiceGridPtr rootNode = new ChoiceGrid(dimension, currentPos);
  ChoiceGridPtr currentNode = rootNode;
  RuleSet activeRules;
  if(dimensions.empty()){
    JTRACE("Output of 0D matrix");
    currentNode->finalizeConstruction(FormulaInteger::one(), allowedRules);
    currentNode->applyRulePriorities();
    return rootNode;
  }
  const RuleDescriptorList& boundaries = dimensions[dimension];
  ChoiceGridPtr lastNode;

  for(RuleDescriptorList::const_iterator i=boundaries.begin(); i!=boundaries.end(); ++i){
    const RuleDescriptor& rd = *i;
    //whenever current pos changes:
    if(!rd.isSamePosition(currentPos)){
//       JTRACE("new pos")(dimension)(rd.getPosition());
      if(lastNode)
        lastNode->_nextElement = currentNode;
      currentPos = rd.getPosition();
      currentNode->finalizeConstruction(currentPos, activeRules);
      if(dimension+1 < dimensions.size())
        currentNode->_nextDimension = constructFrom(activeRules, dimensions, dimension+1);
      else
        currentNode->applyRulePriorities();
      //make next element
      lastNode = currentNode;
      currentNode = new ChoiceGrid(dimension, currentPos);
    }
    if(allowedRules.find(rd.rule()) != allowedRules.end()){
      if(rd.isBegin()){ //add new rule 
        activeRules.insert(rd.rule());
//         JTRACE("begin")(dimension)(rd.rule()->id());
      }else{ //remove expired rule
        activeRules.erase(rd.rule());
//         JTRACE("end")(dimension)(rd.rule()->id());
      }
    }
  }
  JWARNING(activeRules.empty());
  return rootNode;
}


void hecura::ChoiceGrid::print(std::ostream& os) const {
  jalib::ConstMap(&Rule::printIdentifier, os, _applicableRules);
}

// void hecura::ChoiceGrid::generateCodeSimple(CodeGenerator& o, const SimpleRegionPtr& prefix){
//   SimpleRegionPtr tmp = new SimpleRegion(prefix);
//   tmp->addDimension(_begin, _end);
// 
//   if(_nextDimension){
//     _nextDimension->generateCodeSimple(o, tmp);
//   }else{
//     (*_applicableRules.begin())->generateCallCodeSimple(o, tmp);
//   }
// 
//   if(_nextElement) _nextElement->generateCodeSimple(o,prefix);
// }

void hecura::ChoiceGrid::buildIndex(ChoiceGridIndex& idx, const SimpleRegionPtr& prefix /*= 0*/){
  SimpleRegionPtr tmp = prefix ? new SimpleRegion(prefix) : new SimpleRegion();
  tmp->addDimension(_begin, _end);

  if(_nextDimension)
    _nextDimension->buildIndex(idx, tmp);
  else
    idx[tmp] = this;

  if(_nextElement) _nextElement->buildIndex(idx, prefix);
}

void hecura::ChoiceGrid::finalizeConstruction(const FormulaPtr& end, const RuleSet& applicable){
  _end=end;
  _applicableRules = applicable;
}

void hecura::ChoiceGrid::applyRulePriorities(){
  RuleSet rules;
  RuleFlags::PriorityT pri = RuleFlags::PRIORITY_MAX;
  for(RuleSet::const_iterator i=_applicableRules.begin(); i!=_applicableRules.end(); ++i){
    RulePtr r = *i;
    JTRACE("Apply Priorities")(r->id())(r->priority());
    if(r->priority() < pri){
      rules.clear();
      pri=r->priority();
    }
    if(r->priority()==pri)
      rules.insert(r);
  }
  _applicableRules.swap(rules);//faster than a copy
}

