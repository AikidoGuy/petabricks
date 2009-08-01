/***************************************************************************
 *   Copyright (C) 2008 by Jason Ansel                                     *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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
#ifndef PETABRICKSTRANSFORM_H
#define PETABRICKSTRANSFORM_H

#include "jrefcounted.h"
#include "jprintable.h"
#include "matrixdef.h"
#include "choicegrid.h"
#include "rule.h"
#include "learner.h"
#include "performancetester.h"
#include "staticscheduler.h"

#include <vector>
#include <set>
#include <limits>

namespace petabricks {

class Transform;
class TemplateArg;
typedef jalib::JRef<Transform> TransformPtr;
typedef jalib::JRef<TemplateArg> TemplateArgPtr;
class TransformList: public std::vector<TransformPtr>, public jalib::JRefCounted {};
class TemplateArgList: public std::vector<TemplateArgPtr>, public jalib::JRefCounted {};
typedef jalib::JRef<TransformList> TransformListPtr;
typedef jalib::JRef<TemplateArgList> TemplateArgListPtr;
typedef std::set<std::string> ConstantSet;

class DoubleList: public std::vector<double>, public jalib::JRefCounted {};

class TemplateArg : public jalib::JRefCounted, public jalib::JPrintable {
public:
  TemplateArg(std::string name, int min, int max)
    :_name(name), _min(min), _max(max) {
    JASSERT(max>=min)(min)(max);
  }
  void print(std::ostream& o) const { o << _name << "(" << _min << ", " << _max << ")"; }
  
  const std::string& name() const { return _name; }
  int min() const { return _min; }
  int max() const { return _max; }
  int range() const { return _max-_min+1; }
private:
  std::string _name;
  int _min;
  int _max;
};

class ConfigItem {
public:
  ConfigItem(int flags, std::string name, int initial, int min, int max)
      :_flags(flags),
       _name(name),
       _initial(initial),
       _min(min),
       _max(max)
  {}
  
  std::string name     () const { return _name;     }
  int         initial  () const { return _initial;  }
  int         min      () const { return _min;      }
  int         max      () const { return _max;      }

  std::string category() const {
    std::string cat;

    if(hasFlag(ConfigItem::FLAG_USER))
      cat+="user.";
    else
      cat+="system.";

    if(hasFlag(ConfigItem::FLAG_TUNABLE))
      cat+="tunable";
    else
      cat+="config";

    return cat;
  }

  enum FlagT {
    FLAG_TUNABLE       = 1, 
    FLAG_USER          = 2,
    FLAG_SIZE_SPECIFIC = 4
  };
  bool hasFlag(FlagT f) const {
    return (_flags & f) != 0;
  }
private:
  int         _flags;
  std::string _name;
  int         _initial;
  int         _min;
  int         _max;
};

typedef std::vector<ConfigItem> ConfigItems;


/**
 * a transformation algorithm
 */
class Transform : public jalib::JRefCounted, public jalib::JPrintable {
public:
  ///
  /// Constructor
  Transform() :_isMain(false),_tuneId(0),_usesSplitSize(false) {}
  
  //called durring parsing:
  void setName(const std::string& str) { _originalName=_name=str; }
  void addFrom(const MatrixDefList&);
  void addThrough(const MatrixDefList&);
  void addTo(const MatrixDefList&);
  void setRules(const RuleList&);
  
  ///
  /// Initialize after parsing
  void initialize();

  void compile();

  void print(std::ostream& o) const;

  const std::string& name() const { return _name; }
  
  MatrixDefPtr lookupMatrix(const std::string& name) const{
    MatrixDefMap::const_iterator i = _matrices.find(name);
    JASSERT(i != _matrices.end())(name).Text("Unknown input/output matrix");
    return i->second;
  }

  void generateCode(CodeGenerator& o);

  void generateCodeSimple(CodeGenerator& o);
  
  
  void registerMainInterface(CodeGenerator& o);

  void generateMainInterface(CodeGenerator& o);

  void fillBaseCases(const MatrixDefPtr& matrix);
  
  const FreeVars& constants() const { return _constants; }
  FreeVars& constants() { return _constants; }

  void extractSizeDefines(CodeGenerator& o, FreeVars fv);

  void markMain() { _isMain=true; }

  Learner& learner() { return _learner; }
  //PerformanceTester& tester() { return _tester; }

  //void addTestCase(const TestCasePtr& p) {tester().addTestCase(p);}

  std::vector<std::string> maximalArgList() const;

  std::string createTunerPrefix(){
    return _name + "_" + jalib::XToString(_tuneId++) + "_";
  }

  int ruleIdOffset() const { return _rules.front()->id()-1; }

  std::string taskname() const { return _name+"_fin"; }

  void addTemplateArg(const TemplateArgList& args){
    _templateargs.insert(_templateargs.end(), args.begin(), args.end());
  }

  std::vector<std::string> spawnArgs() const;
  std::vector<std::string> spawnArgNames() const;
  std::vector<std::string> normalArgs() const;
  std::vector<std::string> normalArgNames() const;

  void genTmplJumpTable(CodeGenerator& o,
                        bool isStatic,
                        const std::vector<std::string>& args,
                        const std::vector<std::string>& argNames);
  
  void extractConstants(CodeGenerator& o);

  int tmplChoiceCount() const;

  bool isTemplate() const { return !_templateargs.empty(); }

  std::string tmplName(int n, CodeGenerator* o=NULL) const;

  void addUserConfig(const std::string& n, int initial, int min=0, int max=std::numeric_limits<int>::max()){
    addConfigItem(ConfigItem::FLAG_USER,n,initial, min,max);
  }
  
  void addUserTunable(const std::string& n, int initial, int min=0, int max=std::numeric_limits<int>::max()){
    addConfigItem(ConfigItem::FLAG_USER|ConfigItem::FLAG_TUNABLE,n,initial, min,max);
  }
  
  void addConfigItem(int flags, const std::string& n, int initial, int min=0, int max=std::numeric_limits<int>::max()){
    _config.push_back(ConfigItem(flags,n,initial, min,max));
  }

  std::string instClassName() const { return _name+"_instance"; }

  void markSplitSizeUse(CodeGenerator& o);

  void expandWhereClauses(RuleSet&, const MatrixDefPtr&, const SimpleRegionPtr&);

  void addParams(const OrderedFreeVars& p) { _parameters.insert(_parameters.end(), p.begin(), p.end()); }

  
  MatrixDefList defaultVisibleInputs() const {
    MatrixDefList tmp;
    for(MatrixDefList::const_iterator i=_from.begin(); i!=_from.end(); ++i){
      if( (*i)->numDimensions() == 0 )
        tmp.push_back(*i);
    }
    return tmp;
  }

  bool isMain() const { return _isMain; }

  void setAccuracyMetric(const std::string& str){
    JASSERT(_accuracyMetric=="")(_name).Text("accuracy_metric declared twice");
    _accuracyMetric=str;
  }
  void setAccuracyBins(const std::vector<double>& v){
      _accuracyBins = v;
  }
  void addAccuracyVariable(const std::string& s){
      _accuracyVariables.insert(s);
  }
  void addAccuracyVariable(const FreeVars& v){
      _accuracyVariables.insert(v.begin(), v.end());
  }
  void addAccuracyVariable(const OrderedFreeVars& v){
      _accuracyVariables.insert(v.begin(), v.end());
  }

  void setGenerator(const std::string& str){
    JASSERT(_generator=="")(_name).Text("generator declared twice");
    _generator=str;
  }
    
  std::vector<std::string> argnames() const {
    std::vector<std::string> args;
    for(MatrixDefList::const_iterator i=_to.begin(); i!=_to.end(); ++i){
      args.push_back((*i)->name());
    }
    for(MatrixDefList::const_iterator i=_from.begin(); i!=_from.end(); ++i){
      args.push_back((*i)->name());
    }
    return args;
  }


  void addConstant(const std::string& c) { _constants.insert(c); }

private:
  std::string     _originalName;
  std::string     _name;
  MatrixDefList   _from;
  MatrixDefList   _through;
  MatrixDefList   _to;
  MatrixDefMap    _matrices;
  RuleList        _rules;
  ChoiceGridMap   _baseCases;
  FreeVars            _constants;
  OrderedFreeVars _parameters;
  bool            _isMain;
  Learner         _learner;
  StaticSchedulerPtr _scheduler;
  //PerformanceTester  _tester;
  TemplateArgList    _templateargs;
  int                _tuneId;
  ConfigItems        _config;
  bool               _usesSplitSize;
  std::string         _accuracyMetric;
  FreeVars            _accuracyVariables;
  std::vector<double> _accuracyBins;
  std::string        _generator;
};

}

#endif
