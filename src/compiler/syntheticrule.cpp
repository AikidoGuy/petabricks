/***************************************************************************
 *  Copyright (C) 2008-2009 Massachusetts Institute of Technology          *
 *                                                                         *
 *  This source code is part of the PetaBricks project and currently only  *
 *  available internally within MIT.  This code may not be distributed     *
 *  outside of MIT. At some point in the future we plan to release this    *
 *  code (most likely GPL) to the public.  For more information, contact:  *
 *  Jason Ansel <jansel@csail.mit.edu>                                     *
 *                                                                         *
 *  A full list of authors may be found in the file AUTHORS.               *
 ***************************************************************************/
#include "syntheticrule.h"

#include "codegenerator.h"
#include "iterationorders.h"
#include "maximawrapper.h"
#include "transform.h"

void petabricks::SyntheticRule::compileRuleBody(Transform&, RIRScope&){}
void petabricks::SyntheticRule::initialize(Transform&){}

petabricks::RuleFlags::PriorityT petabricks::SyntheticRule::priority() const { 
  return RuleFlags::PRIORITY_DEFAULT; 
}
bool petabricks::SyntheticRule::isRecursive() const { 
  return true;
}
bool petabricks::SyntheticRule::hasWhereClause() const { 
  return false; 
}
petabricks::FormulaPtr petabricks::SyntheticRule::getWhereClause() const { 
  return NULL; 
}

std::string petabricks::SyntheticRule::getLabel() const {
  return "synthetic";
}

bool petabricks::SyntheticRule::canProvide(const MatrixDefPtr&) const { 
  UNIMPLEMENTED(); 
  return false;
}

void petabricks::SyntheticRule::getApplicableRegionDescriptors(RuleDescriptorList&, const MatrixDefPtr&, int, const RulePtr&) { 
  UNIMPLEMENTED(); 
}

void petabricks::SyntheticRule::generateCallCodeSimple(Transform&, CodeGenerator&, const SimpleRegionPtr&){}
void petabricks::SyntheticRule::generateCallTaskCode(const std::string&, Transform&, CodeGenerator&, const SimpleRegionPtr&) {}
void petabricks::SyntheticRule::generateDeclCodeSimple(Transform&, CodeGenerator&) {}
void petabricks::SyntheticRule::generateTrampCodeSimple(Transform&, CodeGenerator&) {}

void petabricks::SyntheticRule::markRecursive() { 
  UNIMPLEMENTED();
}
const petabricks::FormulaPtr& petabricks::SyntheticRule::recursiveHint() const { 
  return FormulaPtr::null();
}
void petabricks::SyntheticRule::print(std::ostream& os) const {
  os << "SyntheticRule " << _id << std::endl;;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//TODO: consider new file for WhereExpansionRule

void petabricks::WhereExpansionRule::generateCallCodeSimple( Transform&
                                                           , CodeGenerator& o
                                                           , const SimpleRegionPtr& region){
  o.callSpatial(codename()+TX_STATIC_POSTFIX, region);
}

void petabricks::WhereExpansionRule::generateCallTaskCode( const std::string& name
                                                         , Transform& trans
                                                         , CodeGenerator& o
                                                         , const SimpleRegionPtr& region){
  o.mkSpatialTask(name, trans.instClassName(), codename()+TX_STATIC_POSTFIX, region);
}
  

void petabricks::WhereExpansionRule::generateTrampCodeSimple(Transform& trans, CodeGenerator& o){
  //for now static only:
  IterationDefinition iterdef(*this, getSelfDependency() , false);
  std::vector<std::string> packedargs = iterdef.packedargs();
  o.beginFunc("petabricks::DynamicTaskPtr", codename()+TX_STATIC_POSTFIX, packedargs);
  iterdef.unpackargs(o);
  iterdef.genLoopBegin(o);
  genWhereSwitch(trans,o);
  iterdef.genLoopEnd(o);
  o.write("return NULL;");
  o.endFunc();
}


void petabricks::WhereExpansionRule::genWhereSwitch(Transform& trans, CodeGenerator& o){
  RuleSet::iterator i;
  for(i=_rules.begin(); i!=_rules.end(); ++i){
    for(int d=0; d<(*i)->dimensions(); ++d){
      o._define((*i)->getOffsetVar(d)->toString(), getOffsetVar(d)->toString());
    }

    FormulaPtr wc = (*i)->getWhereClause();
    if(!wc)
      o.elseIf();
    else if(i==_rules.begin())
      o.beginIf(wc->toCppString());
    else
      o.elseIf(wc->toCppString());

    (*i)->generateTrampCellCodeSimple(trans, o, true);
    
    for(int d=0; d<(*i)->dimensions(); ++d){
      o._undefine((*i)->getOffsetVar(d)->toString());
    }

    if(!wc){
      o.endIf();
      return; //we reached an unconditioned rule
    }
  }
  o.elseIf();
  o.write("JASSERT(false).Text(\"All where clauses failed, no rule to compute region\");");
  o.endIf();
}


bool petabricks::WhereExpansionRule::isSingleElement() const { 
  return false;
}

int petabricks::WhereExpansionRule::dimensions() const {
  RuleSet::const_iterator i=_rules.begin();
  int rv = (*i)->dimensions();
  for(++i ;i!=_rules.end(); ++i)
    JASSERT(rv==(*i)->dimensions())(rv)((*i)->dimensions())
      .Text("where clauses only work with common number of dimensions");;
  return rv;
}
petabricks::DependencyDirection petabricks::WhereExpansionRule::getSelfDependency() const {
  DependencyDirection rv;
  RuleSet::const_iterator i;
  for(i=_rules.begin(); i!=_rules.end(); ++i)
    rv.addDirection((*i)->getSelfDependency());
  return rv;
}
petabricks::FormulaPtr petabricks::WhereExpansionRule::getSizeOfRuleIn(int d) {
  RuleSet::const_iterator i=_rules.begin();
  FormulaPtr rv = (*i)->getSizeOfRuleIn(d);
  for(++i ;i!=_rules.end(); ++i)
    JASSERT(MAXIMA.compare(rv,"=", (*i)->getSizeOfRuleIn(d)))
      .Text("where clauses only work with common sizes in each choice");;
  return rv;
}

std::string petabricks::WhereExpansionRule::codename() const {
  return "whereExpansion"+jalib::XToString(_id); 
}

void petabricks::WhereExpansionRule::collectDependencies(StaticScheduler& scheduler) { 
  RuleSet::const_iterator i;
  for(i=_rules.begin(); i!=_rules.end(); ++i)
    (*i)->collectDependencies(scheduler);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//TODO: consider new file for DuplicateExpansionRule

void petabricks::DuplicateExpansionRule::generateCallCodeSimple( Transform& trans
                                                           , CodeGenerator& o
                                                           , const SimpleRegionPtr& region){
  size_t old = _rule->setDuplicateNumber(_dup);
  _rule->generateCallCodeSimple(trans, o, region);
  _rule->setDuplicateNumber(old);
}

void petabricks::DuplicateExpansionRule::generateCallTaskCode( const std::string& name
                                                         , Transform& trans
                                                         , CodeGenerator& o
                                                         , const SimpleRegionPtr& region){
  size_t old = _rule->setDuplicateNumber(_dup);
  _rule->generateCallTaskCode(name, trans, o, region);
  _rule->setDuplicateNumber(old);
}
  

void petabricks::DuplicateExpansionRule::generateTrampCodeSimple(Transform& trans, CodeGenerator& o){
  size_t old = _rule->setDuplicateNumber(_dup);
  _rule->generateTrampCodeSimple(trans, o);
  _rule->setDuplicateNumber(old);
}


bool petabricks::DuplicateExpansionRule::isSingleElement() const { 
  return _rule->isSingleElement();
}

int petabricks::DuplicateExpansionRule::dimensions() const {
  return _rule->dimensions();
}
petabricks::DependencyDirection petabricks::DuplicateExpansionRule::getSelfDependency() const {
  return _rule->getSelfDependency();
}
petabricks::FormulaPtr petabricks::DuplicateExpansionRule::getSizeOfRuleIn(int d) {
  return _rule->getSizeOfRuleIn(d);
}

std::string petabricks::DuplicateExpansionRule::codename() const {
  return "duplicateExpansion"+jalib::XToString(_id); 
}

void petabricks::DuplicateExpansionRule::collectDependencies(StaticScheduler& scheduler) { 
  _rule->collectDependencies(scheduler);
}

petabricks::RuleFlags::PriorityT petabricks::DuplicateExpansionRule::priority() const { 
  return _rule->priority();
}
bool petabricks::DuplicateExpansionRule::isRecursive() const { 
  return _rule->isRecursive();
}
bool petabricks::DuplicateExpansionRule::hasWhereClause() const { 
  return _rule->hasWhereClause();
}
petabricks::FormulaPtr petabricks::DuplicateExpansionRule::getWhereClause() const { 
  return _rule->getWhereClause();
}
bool petabricks::DuplicateExpansionRule::canProvide(const MatrixDefPtr& md) const { 
  return _rule->canProvide(md);
}
void petabricks::DuplicateExpansionRule::getApplicableRegionDescriptors(RuleDescriptorList& rdl, const MatrixDefPtr& md, int i, const RulePtr& rule) { 
  _rule->getApplicableRegionDescriptors(rdl, md, i, rule);
}
const petabricks::FormulaPtr& petabricks::DuplicateExpansionRule::recursiveHint() const { 
  return _rule->recursiveHint();
}

