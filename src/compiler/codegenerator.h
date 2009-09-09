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
#ifndef PETABRICKSCODEGENERATOR_H
#define PETABRICKSCODEGENERATOR_H

#include "formula.h"
#include "trainingdeps.h"

#include "common/jconvert.h"
#include "common/jprintable.h"
#include "common/jrefcounted.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace petabricks {

typedef std::map<std::string, std::string> TunableDefs;

class CodeGenerator;
typedef jalib::JRef<CodeGenerator> CodeGeneratorPtr;
typedef std::vector<CodeGeneratorPtr> CodeGenerators;

class TaskCodeGenerator;

class CodeGenerator : public jalib::JRefCounted {
public:
  struct ClassMember {
    std::string type;
    std::string name;
    std::string initializer;
    static const char* PASSED() { return "PASSED"; }
  };
  typedef std::vector<ClassMember> ClassMembers;

  
  static std::stringstream& theFilePrefix();
  static TunableDefs& theTunableDefs();

  void incIndent(){++_indent;}
  void decIndent(){--_indent;}
  
  CodeGenerator();
  virtual ~CodeGenerator(){}

  void beginFor(const std::string& var, const FormulaPtr& begin, const FormulaPtr& end,  const FormulaPtr& step);
  void beginReverseFor(const std::string& var, const FormulaPtr& begin, const FormulaPtr& end,  const FormulaPtr& step);
  void endFor();

  void call(const std::string& func, const std::string& args);
  void call(const std::string& func, const std::vector<std::string>& args);
  void setcall(const std::string& lv, const std::string& func, const std::vector<std::string>& args);

  virtual void beginFunc(const std::string& rt, const std::string& func, const std::vector<std::string>& args = std::vector<std::string>());
  void endFunc();

  void varDecl(const std::string& var);
  void addAssert(const std::string& l, const std::string& r);

  void write(const std::string& str);
  void newline();

  void comment(const std::string& str);

  void beginIf(const std::string& v);
  void beginIfNot(const std::string& v){
    beginIf(" ! ("+v+") ");
  }
  void elseIf(const std::string& v = "true");
  void endIf();

  void createTunable( bool isTunable
                    , const std::string& category
                    , const std::string& name
                    , int initial
                    , int min=0
                    , int max=std::numeric_limits<int>::max())
  {
    //JTRACE("new tunable")(name)(initial)(min)(max);
    theTunableDefs()[name] =
       "JTUNABLE("+name
              +","+jalib::XToString(initial)
              +","+jalib::XToString(min)
              +","+jalib::XToString(max)+")";
    _cg.addTunable( isTunable, category, name, initial, min, max);
  }
  void createTunableArray(const std::string& category
                    , const std::string& name
                    , int count
                    , int initial
                    , int min=0
                    , int max=std::numeric_limits<int>::max())
  {
    //JTRACE("new tunable")(name)(initial)(min)(max);
    theTunableDefs()[name] =
       "JTUNABLEARRAY("+name
              +","+jalib::XToString(count)
              +","+jalib::XToString(initial)
              +","+jalib::XToString(min)
              +","+jalib::XToString(max)+")";
    _cg.addTunable( false, category, name, initial, min, max);
  }

  void beginSwitch(const std::string& var){
    write("switch("+var+"){");
  }
  void endSwitch(){
    write("}");
  }
  void beginCase(int n){
    _indent++;
    write("case "+jalib::XToString(n)+": {");
    _indent++;
  }
  void endCase(){
    _indent--;
    write("}");
    write("break;");
    _indent--;
  }

  TrainingDeps& cg() { return _cg; }


  void beginClass(const std::string& name, const std::string& base);
  void endClass();
  void addMember(const std::string& type, const std::string& name, const std::string& initializer = ClassMember::PASSED());
  void continuationPoint();
  void continuationRequired(const std::string& prereq);
  std::string nextContName(const std::string& base);
  
  void continueLabel(const std::string& fn){
    continueJump(fn);
    endFunc();
    beginFunc("petabricks::DynamicTaskPtr", fn);
  }
  void continueJump(const std::string& fn){
    write("return "+fn+"();");
  }

  void define(const std::string& name, const std::string& val){
    _defines.push_back(name);
    _define(name,val);
  }

  void _define(const std::string& name, const std::string& val){
    write("#define "+name+" "+val);
  }
  void _undefine(const std::string& name){
    write("#undef "+name);
  }

  void undefineAll(){
    for(size_t i=0; i<_defines.size(); ++i)
      write("#undef "+_defines[i]);
    _defines.clear();
  }
  bool inClass() const { return _curClass.size()>0; }

  void constructorBody(const std::string& s){_curConstructorBody=s;}

  void staticMember() { hos() << "static "; }

  void withEachMember(const std::string& type, const std::string& code){
    for(size_t i=0; i<_curMembers.size(); ++i)
      if(_curMembers[i].type == type)
        write(_curMembers[i].name+code+";");
  }

  void globalDefine(const std::string& n, const std::string& v){
    dos() << "#define " << n << " " << v << "\n";
  }

  ///
  /// Create a parallel code generator 
  /// (for writing other function bodies before current function is finished)
  CodeGenerator& forkhelper();

  ///
  /// Write the bodies from any calles to forkhelp
  void mergehelpers();
  
  void outputFileTo(std::ostream& o){
    o << theFilePrefix().str();
    o << "\n// global defines \n";
    o << _dos.str();
    o << "\n// Tunable declarations\n";
    for(TunableDefs::const_iterator i=theTunableDefs().begin(); i!=theTunableDefs().end(); ++i)
      o << i->second << ";\n";
    o << "\n\n//////////////////////////////////////////////////////////////////////\n\n";
    o << _hos.str();
    o << "\n\n//////////////////////////////////////////////////////////////////////\n\n";
    o << _os.str();
  }
protected:
  void indent();
  std::ostream& hos() { return _hos; }
  std::ostream& dos() { return _dos; }
  std::ostream& os()  { return _os; }
protected:
  std::vector<std::string> _defines;
  ClassMembers   _curMembers;
  std::string    _curClass;
  std::string    _curConstructorBody;
  int            _contCounter;
  int            _indent;
  TrainingDeps   _cg;
  CodeGenerators _helpers;
  std::ostringstream _os;
  std::ostringstream _hos;
  std::ostringstream _dos;
};

}

#endif
