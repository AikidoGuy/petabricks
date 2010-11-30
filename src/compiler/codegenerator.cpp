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
#include "codegenerator.h"

#include "region.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

std::stringstream& petabricks::CodeGenerator::theFilePrefix() { 
  static std::stringstream t; 
  return t; 
}

petabricks::TunableDefs& petabricks::CodeGenerator::theTunableDefs() {
  static TunableDefs t;
  return t;
}

jalib::TunableValueMap& petabricks::CodeGenerator::theHardcodedTunables() {
  static jalib::TunableValueMap t;
  return t;
}

petabricks::CodeGenerator::CodeGenerator(const StreamTreePtr& root) : _contCounter(0), _indent(0) {
  _oheaders = root->add(new StreamTree("headers"));
  _odefines = root->add(new StreamTree("defines"));
  _bcur     = root->add(new StreamTree("top", root.asPtr()));
  _ocur = _bcur->add(new StreamTree("main"));
}


petabricks::CodeGenerator::CodeGenerator(CodeGenerator& that)
  : jalib::JRefCounted(), _contCounter(0), _indent(0)
{
  _oheaders = that._oheaders;
  _odefines = that._odefines;
  _bcur     = that._bcur;
  _ocur     = new StreamTree("childsection", _bcur.asPtr());
  _bcur->add(_ocur.asPtr());
  _curClass = that._curClass;
}


void petabricks::CodeGenerator::beginFor(const std::string& var, const FormulaPtr& begin, const FormulaPtr& end,  const FormulaPtr& step){
  indent();
  os() << "for(int " << var << "=" << begin << "; "<< var << "<" << end << "; " << var << "+="<< step <<" ){\n";
  _indent++;
}

void petabricks::CodeGenerator::beginReverseFor(const std::string& var, const FormulaPtr& begin, const FormulaPtr& end,  const FormulaPtr& step){
  indent();
  os() << "for(int " << var << "=" << end->minusOne() << "; "<< var << ">=" << begin << "; " << var << "-="<< step <<" ){\n";
  _indent++;
}

void petabricks::CodeGenerator::endFor(){
  _indent--;
    indent();
  os() << "}\n";
}

void petabricks::CodeGenerator::call(const std::string& func, const std::string& args){
  indent();
  os() << func << '(' << args << ");\n";
}

void petabricks::CodeGenerator::call(const std::string& func, const std::vector<std::string>& args){
  indent();
  os() << func << '(';
  jalib::JPrintable::printStlList(os(), args.begin(), args.end(), ", ");
  os() << ");\n";
}

void petabricks::CodeGenerator::setcall(const std::string& lv, const std::string& func, const std::vector<std::string>& args){
  indent();
  os() << lv << " = " << func << '(';
  jalib::JPrintable::printStlList(os(), args.begin(), args.end(), ", ");
  os() << ");\n";
}

void petabricks::CodeGenerator::beginFunc(const std::string& rt, const std::string& func, const std::vector<std::string>& args, bool is_static){
  indent();
  os() << rt << " ";
  if(inClass()) os() << _curClass << "::";
  os() << func << '(';
  jalib::JPrintable::printStlList(os(), args.begin(), args.end(), ", ");
  os() << "){\n";
  _indent++;

  if(inClass()) hos() << "  ";
  if(is_static) hos() << "static ";
  hos() << rt << " " << func << '(';
  jalib::JPrintable::printStlList(hos(), args.begin(), args.end(), ", ");
  hos() << ");\n";
  
  if(!inClass()) hos() << "\n";
}

void petabricks::CodeGenerator::varDecl(const std::string& var){
  indent();
  os() << var << ";\n";
} 

void petabricks::CodeGenerator::addAssert(const std::string& l, const std::string& r){
  indent();
  os() << "JASSERT("<<l<<" == "<<r<<")"
    << "(" << l << ")"
    << "(" << r << ")"
    << ";\n";
} 

void petabricks::CodeGenerator::endFunc(){
  _indent--;
  indent();
  os() << "}\n";
}

void petabricks::CodeGenerator::indent(){ 
  if(_indent>0)
    os() << std::string(_indent*2,' '); 
}

void petabricks::CodeGenerator::write(const std::string& str){
  indent();
  os() << str << "\n";
}

void petabricks::CodeGenerator::newline(){
  os() << "\n";
}

void petabricks::CodeGenerator::comment(const std::string& str){
  indent();
  os() << "// " << str << "\n";
}

void petabricks::CodeGenerator::beginIf(const std::string& v){
  indent();
  os() << "if(" << v << "){\n";
  _indent++;
}

void petabricks::CodeGenerator::elseIf(const std::string& v /*= "true"*/){
  _indent--;
  indent();
  if(v=="true")
    os() << "}else{\n";
  else
    os() << "}else if(" << v << "){\n";
  _indent++;
}

void petabricks::CodeGenerator::endIf(){
  _indent--;
    indent();
  os() << "}\n";
}

namespace{//file local
  void _splitTypeArgs(std::string& type, std::string& name, const std::string& str){
    const char* begin=str.c_str();
    const char* mid=begin+str.length();
    const char* end=mid;
    while(mid>begin && *(--mid)!=' ') ;
//     JTRACE("SPLIT")(begin)(mid+1);
    JASSERT(mid!=begin);
    type.assign(begin,mid);
    name.assign(mid+1,end);
  }
}

static std::string _typeToConstRef(std::string s){
  if(s[s.length()-1] != '&'){
    s+='&';
    if(   s[0]=='c'
       && s[1]=='o'
       && s[2]=='n'
       && s[3]=='s'
       && s[4]=='t'
       && s[5]==' '){
      return s;
    }
    s="const "+s;
  }
  return s;
}

void petabricks::CodeGenerator::beginClass(const std::string& name, const std::string& base){
  hos() << "class " << name << " : public " << base << " {\n";
  hos() << ("  typedef "+base+" BASE;\npublic:\n");
  _curClass=name;
  _contCounter=0;
  JASSERT(_curMembers.empty())(_curMembers.size());
}
void petabricks::CodeGenerator::endClass(){
  const char* delim="";
  indent();
  os() << _curClass << "::" << _curClass << "(";
  hos() << _curClass << "(";
  for(ClassMembers::const_iterator i=_curMembers.begin(); i!=_curMembers.end(); ++i){
    if(i->initializer == ClassMember::PASSED()){
      os() << delim << _typeToConstRef(i->type) <<" t_"<<i->name;
      hos() << delim << _typeToConstRef(i->type) <<" t_"<<i->name;
      delim=", ";
    }
  }
  os() << ")\n";
  hos() << ");\n";
  indent();
  os() << "  : BASE()";
  for(ClassMembers::const_iterator i=_curMembers.begin(); i!=_curMembers.end(); ++i){
    if(i->initializer == ClassMember::PASSED())
      os() << ", " << i->name<<"(t_"<<i->name<<")";
    else if(i->initializer.size()>0)
      os() << ", " << i->name<<"("<<i->initializer<<")";
  }
  newline();
  write("{"+_curConstructorBody+"}");
  _curConstructorBody="";
  hos() << "//private:\n";
  for(ClassMembers::const_iterator i=_curMembers.begin(); i!=_curMembers.end(); ++i){
    hos() << "  " << i->type << " " << i->name <<";\n";
  }
  _curMembers.clear();
  _curClass="";
  hos() << "};\n\n";
  newline();
  newline();
}
void petabricks::CodeGenerator::addMember(const std::string& type, const std::string& name, const std::string& initializer){
  if(_curClass.size()>0){
    ClassMember tmp;
    tmp.type=type;
    tmp.name=name;
    tmp.initializer=initializer;
    _curMembers.push_back(tmp);
  }else{
    if(initializer.size()>0)
      varDecl(type+" "+name+" = "+initializer);
    else
      varDecl(type+" "+name);
  }
}

std::string petabricks::CodeGenerator::nextContName(const std::string& base){
  return base+"cont_" + jalib::XToString(_contCounter++);
}

void petabricks::CodeGenerator::continuationPoint(){
#ifndef DISABLE_CONTINUATIONS
  std::string n = "cont_" + jalib::XToString(_contCounter++);
  beginIf("useContinuation()");
  write("return new petabricks::MethodCallTask<"+_curClass+", &"+_curClass+"::"+n+">( this );"); 
  elseIf();
  write("return "+n+"();"); 
  endIf();
  endFunc();
  beginFunc("DynamicTaskPtr", n);
#endif
}

void petabricks::CodeGenerator::continuationRequired(const std::string& hookname){
  std::string n = "cont_" + jalib::XToString(_contCounter++);
  newline();
  write("return "+hookname+" new petabricks::MethodCallTask<"+_curClass+", &"+_curClass+"::"+n+">(this));"); 
  endFunc();
  beginFunc("DynamicTaskPtr", n);
}


petabricks::CodeGenerator& petabricks::CodeGenerator::forkhelper(){
  CodeGenerator* cg;
  _helpers.push_back(cg=new CodeGenerator(*this));
  return *cg;
}

void petabricks::CodeGenerator::mergehelpers(){
  for(; !_helpers.empty(); _helpers.pop_back()){
    JASSERT(_helpers.front()->_defines.empty())
      .Text("helper CodeGenerator leaked defines");
   //_helpers.front()->os().writeTo(os());
   //_helpers.front()->hos().writeTo(hos());
   //_helpers.front()->dos().writeTo(dos());
  }
}
void petabricks::CodeGenerator::callSpatial(const std::string& methodname, const SimpleRegion& region) {
  write("{");
  incIndent();
  write("IndexT _tmp_begin[] = {" + region.minCoord().toString() + "};");
  write("IndexT _tmp_end[] = {"   + region.maxCoord().toString() + "};");
  write(methodname+"(_tmp_begin, _tmp_end);");
  decIndent();
  write("}");
}
void petabricks::CodeGenerator::mkSpatialTask(const std::string& taskname, const std::string& objname, const std::string& methodname, const SimpleRegion& region) {
  std::string taskclass = "petabricks::SpatialMethodCallTask<"+objname
                        + ", " + jalib::XToString(region.dimensions())
                        + ", &" + objname + "::" + methodname
                        + ">";
  write("{");
  incIndent();
  write("IndexT _tmp_begin[] = {" + region.minCoord().toString() + "};");
  write("IndexT _tmp_end[] = {"   + region.maxCoord().toString() + "};");
  write(taskname+" = new "+taskclass+"(this,_tmp_begin, _tmp_end);");
  decIndent();
  write("}");
}



