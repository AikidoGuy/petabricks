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
#ifndef PETABRICKSUSERRULE_H
#define PETABRICKSUSERRULE_H

#include "pbc.h"

#include "codegenerator.h"
#include "clcodegenerator.h"
#include "matrixdependency.h"
#include "rule.h"
#include "ruleir.h"
#include "iterationorders.h"

#include "common/jconvert.h"

#include <vector>

namespace petabricks {

/**
 * Represent a transform rule defined by the user
 */
class UserRule : public RuleInterface{
public:
  ///
  /// Constructor -- return style rule
  UserRule(const RegionPtr& to, const RegionList& from, const FormulaList& where);

  ///
  /// Constructor -- to style rule
  UserRule(const RegionList& to, const RegionList& from, const FormulaList& where);
  
  ///
  /// Initialize this rule after parsing
  void initialize(Transform&);
  
  ///
  /// Set this->_body
  void setBody(const char*);

  ///
  /// Set priority flag
  void setPriority(RuleFlags::PriorityT v)  { _flags.priority = v; }
  
  ///
  /// Set rotation flag
  void addRotations(RuleFlags::RotationT v) { _flags.rotations |= v; }

  ///
  /// Print this rule to a given stl stream
  /// implements JPrintable::print
  void print(std::ostream& o) const;
  
  ///
  /// Add RuleDescriptors to output corresponding to the extrema of the applicable region in dimension
  void getApplicableRegionDescriptors(RuleDescriptorList& output, const MatrixDefPtr& matrix, int dimension);

  ///
  /// Generate seqential code to declare this rule
  void generateDeclCodeSimple(Transform& trans, CodeGenerator& o);


  ///
  /// Generate seqential code to declare this rule
  void generateTrampCodeSimple(Transform& trans, CodeGenerator& o, RuleFlavor flavor);
  void generateTrampCodeSimple(Transform& trans, CodeGenerator& o){
    generateTrampCodeSimple(trans, o, E_RF_STATIC);
    generateTrampCodeSimple(trans, o, E_RF_DYNAMIC);
    #ifdef HAVE_OPENCL
    generateTrampCodeSimple(trans, o, E_RF_OPENCL);
    #endif
  }
  void generateTrampCellCodeSimple(Transform& trans, CodeGenerator& o, RuleFlavor flavor);

  ///
  /// Generate an OpenCL program implementing this rule
  void generateOpenCLKernel( Transform& trans, CLCodeGenerator& clo, IterationDefinition& iterdef );

  ///
  /// Generate seqential code to invoke this rule
  void generateCallCodeSimple(Transform& trans, CodeGenerator& o, const SimpleRegionPtr& region); 
  void generateCallTaskCode(const std::string& name, Transform& trans, CodeGenerator& o, const SimpleRegionPtr& region);

  ///
  /// Return function the name of this rule in the code
  std::string implcodename(Transform& trans) const;
  std::string trampcodename(Transform& trans) const;

  bool isReturnStyle() const { return _flags.isReturnStyle; }

  int dimensions() const;

  void addAssumptions() const;

  void collectDependencies(StaticScheduler& scheduler);

  void markRecursive() { 
    markRecursive(NULL);
  }
  void markRecursive(const FormulaPtr& rh) { 
    if(!_flags.isRecursive){
      _flags.isRecursive = true; 
      _recursiveHint = rh;
    }
  }

  bool isRecursive() const { return _flags.isRecursive; }

  RuleFlags::PriorityT priority() const { return _flags.priority; }
  const FormulaList& conditions() const { return _conditions; }

  bool canProvide(const MatrixDefPtr& m) const {
    return _provides.find(m) != _provides.end();
  }

  const FormulaPtr& recursiveHint() const { return _recursiveHint; }

  FormulaPtr getSizeOfRuleIn(int d){
    for(size_t i=0; i<_to.size(); ++i){
      if(d < (int)_to[i]->dimensions()){
        return _to[i]->getSizeOfRuleIn(d);
      }
    }
    JASSERT(false)(d)(_id);
    return 0;
  }

  bool isSingleElement() const {
    if(_to.size()!=1) return false;
    return _to[0]->isSingleElement();
  }

  void compileRuleBody(Transform& tx, RIRScope& s);

  bool isSingleCall() const {
    for(size_t i=0; i<_to.size(); ++i)
      if(!_to[i]->isAll())
        return false;
    return true;
  }

  bool hasWhereClause() const { return _conditions.size()>0; }

  FormulaPtr getWhereClause() const { return getWhereClause(0); }
  FormulaPtr getWhereClause(int start) const {
    if(_conditions.size()==0) return NULL;
    if((int)_conditions.size()-1==start) return _conditions[start];
    return new FormulaAnd(_conditions[start], getWhereClause(start+1));
  }
  
  DependencyDirection getSelfDependency() const;
private:
  RuleFlags   _flags;
  RegionList  _from;
  RegionList  _to;
  FormulaList _conditions;
  FormulaList _definitions;
  std::string     _bodysrc;
  RIRBlockCopyRef _bodyirStatic;
  RIRBlockCopyRef _bodyirDynamic;
  MatrixDependencyMap _depends;
  MatrixDependencyMap _provides;
  FormulaPtr          _recursiveHint;
};

}

#endif
