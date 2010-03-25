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
#ifndef PETABRICKSRIRCOMPILERPASS_H
#define PETABRICKSRIRCOMPILERPASS_H

#include "rirscope.h"
#include "ruleir.h"
#include "trainingdeps.h"
#include "userrule.h"

#include "common/jprintable.h"

namespace petabricks {

class CodeGenerator;
class RuleInterface;
class Transform;

class RIRCompilerPass : public RIRVisitor {
protected:
  template<typename T> class Context {
  public:
    typedef std::list<T> TList;
    Context(TList* bk, TList* fw, int l) 
      : _backward(bk), _forward(fw), _lvl(l)
    {}
    TList* backward() const { return _backward;}
    TList* forward () const { return _forward; }
    int    lvl     () const { return _lvl;     }
  private:
    TList* _backward;
    TList* _forward;
    int    _lvl;
  };
  typedef Context<RIRExprCopyRef> ExprContext;
  typedef Context<RIRStmtCopyRef> StmtContext;
  typedef std::vector<ExprContext> ExprContextStack;
  typedef std::vector<StmtContext> StmtContextStack;
  
  void pushExprForward(const RIRExprCopyRef& e){ 
    _exprCtx.back().forward()->push_front(e);
  }
  void pushExprBackward(const RIRExprCopyRef& e){ 
    _exprCtx.back().backward()->push_back(e);
  }
  bool hasExprBackward(){ 
    return !_exprCtx.back().backward()->empty();
  }
  bool hasExprForward(){ 
    return !_exprCtx.back().forward()->empty();
  }
  RIRExprRef peekExprForward() { 
    JASSERT(!_exprCtx.back().forward()->empty()).Text("unexpected end of statement");
    RIRExprRef p = _exprCtx.back().forward()->front().asPtr();
    return p;
  }
  RIRExprRef popExprForward() { 
    RIRExprRef p = peekExprForward();
    _exprCtx.back().forward()->pop_front();
    return p;
  }
  RIRExprRef peekExprBackward() { 
    JASSERT(!_exprCtx.back().backward()->empty()).Text("unexpected end of statement");
    RIRExprRef p = _exprCtx.back().backward()->back().asPtr();
    return p;
  }
  RIRExprRef popExprBackward() { 
    RIRExprRef p = peekExprBackward();
    _exprCtx.back().backward()->pop_back();
    return p;
  }
  void pushStmtForward(const RIRStmtCopyRef& s){ 
    _stmtCtx.back().forward()->push_front(s);
  }
  void pushStmtBackward(const RIRStmtCopyRef& s){ 
    _stmtCtx.back().backward()->push_back(s);
  }

public:
  //void before(RIRExprCopyRef&) {}
  //void before(RIRStmtCopyRef&) {}
  //void before(RIRBlockCopyRef&){}
  //void after(RIRExprCopyRef&)  {}
  //void after(RIRStmtCopyRef&)  {}
  //void after(RIRBlockCopyRef&) {}

  //bool shouldDescend(const RIRNode&) { return true; }

  virtual void pushScope(){}
  virtual void popScope(){}

  virtual void beforeAny(const RIRNodeRef&){}
  virtual void afterAny(const RIRNodeRef&){}
protected:
  int depth() const { return _stack.size(); } 

  RIRCompilerPass(const RIRScopePtr& scope = RIRScope::global()->createChildLayer()) : _scope(scope) {}

  const RIRNodeRef& parentNode() const { JASSERT(depth()>=2)(depth()); return _stack[_stack.size()-2]; }

private:
  void _before(RIRExprCopyRef& p)  { 
    _beforeAny(p.asPtr());
    RIRVisitor::_before(p);
  }
  void _after(RIRExprCopyRef& p)   { 
    RIRVisitor::_after(p);
    _afterAny(p.asPtr());
  }
  
  void _before(RIRStmtCopyRef& p)  { 
    _beforeAny(p.asPtr());
    RIRVisitor::_before(p);
  }
  void _after(RIRStmtCopyRef& p)   { 
    RIRVisitor::_after(p);
    _afterAny(p.asPtr());
  }
 
  void _before(RIRBlockCopyRef& p) { 
    _scope=_scope->createChildLayer();
    pushScope();
    _beforeAny(p.asPtr());
    RIRVisitor::_before(p);
  }
  void _after(RIRBlockCopyRef& p)  { 
    RIRVisitor::_after(p);
    _afterAny(p.asPtr());
    popScope();
    _scope=_scope->parentLayer();
  } 
  void _beforeAny(const RIRNodeRef& n){
    _stack.push_back(n);
    beforeAny(n);
  }
  void _afterAny(const RIRNodeRef& n){
    afterAny(n);
    JASSERT(!_stack.empty());
    _stack.pop_back();
  }
  void pushSplicer(RIRStmtList* bk, RIRStmtList* fwd){
    _stmtCtx.push_back(StmtContext(bk, fwd, depth()));
  }
  void popSplicer(RIRStmtList* /*bk*/, RIRStmtList* /*fwd*/){
    JASSERT(!_stmtCtx.empty());
    _stmtCtx.pop_back();
  }
  void pushSplicer(RIRExprList* bk, RIRExprList* fwd){
    _exprCtx.push_back(ExprContext(bk, fwd, depth()));
  }
  void popSplicer(RIRExprList* /*bk*/, RIRExprList* /*fwd*/){
    JASSERT(!_exprCtx.empty());
    _exprCtx.pop_back();
  }
protected:
  std::vector<RIRNodeRef>_stack;
  ExprContextStack _exprCtx;
  StmtContextStack _stmtCtx;
  RIRScopePtr      _scope;
};

class GpuRenamePass : public RIRCompilerPass {
public:
  GpuRenamePass() : RIRCompilerPass(RIRScope::global()->createChildLayer()) {}
  void before(RIRExprCopyRef& e);
};

class DebugPrintPass : public RIRCompilerPass {
public:
  DebugPrintPass() : RIRCompilerPass(RIRScope::global()->createChildLayer()) {}
  void beforeAny(const RIRNodeRef& n){
    std::cout << std::string(depth()*2, ' ')
              << n->debugStr() << std::endl; 
  }
};

class ExpansionPass : public RIRCompilerPass {
public:
  ExpansionPass(Transform& t, RuleInterface& r, const RIRScopePtr& p) 
    : RIRCompilerPass(p->createChildLayer()), _transform(t), _rule(r)
  {}

  void before(RIRExprCopyRef& e);
  void before(RIRStmtCopyRef& e);
private:
  Transform&     _transform;
  const RuleInterface& _rule;
};

class AnalysisPass: public RIRCompilerPass {
public:
  AnalysisPass(RuleInterface& r, const std::string& name, const RIRScopePtr& p) 
    : RIRCompilerPass(p->createChildLayer()), _name(name), _rule(r)
  {}

  void before(RIRExprCopyRef& e);

private:
  std::string _name;
  RuleInterface& _rule;
};

class LiftVardeclPass : public RIRCompilerPass {
public:
  LiftVardeclPass(Transform& t, RuleInterface& r, CodeGenerator& oo)
    : RIRCompilerPass(), o(oo), _transform(t), _rule(r)
  {}

  void before(RIRExprCopyRef& s);

protected:
  std::string prefix() const {
    std::ostringstream o;
    o << "b";
    if(_prefixStack.size()>1)
      jalib::JPrintable::printStlList(o, _prefixStack.begin(), _prefixStack.end()-1, "b");
    o << "local_";
    return o.str();
  }
  void pushScope(){
    if(_prefixStack.empty())
      _prefixStack.push_back(-1); 
    JTRACE("pushScope")(_prefixStack.size());
    _prefixStack.back()++;
    _prefixStack.push_back(-1);
  }
  void popScope(){
    _prefixStack.pop_back();
    JTRACE("popScope")(_prefixStack.size());
  }
private:
  CodeGenerator& o;
  std::vector< int > _prefixStack;
  const Transform&     _transform;
  const RuleInterface& _rule;
};

class DynamicBodyPrintPass : public RIRCompilerPass {
public:
  DynamicBodyPrintPass(CodeGenerator& oo )
    : RIRCompilerPass(), o(oo)
  {}

  void before(RIRStmtCopyRef& s);

  bool shouldDescend(const RIRNode& n) { return !n.isStmt(); }
private:
  CodeGenerator& o;
  std::vector<std::string> _breakTargets;
  std::vector<std::string> _continueTargets;
};

class OpenClCleanupPass: public RIRCompilerPass {
public:
  class NotValidSource {};
  OpenClCleanupPass(UserRule& r, const RIRScopePtr& p) 
    : RIRCompilerPass(p->createChildLayer()), _rule(r)
  {}
  void before(RIRExprCopyRef& e);
private:
  RegionPtr findMatrix(std::string var);
  void generateAccessor( const RegionPtr& region, const FormulaPtr& x, const FormulaPtr& y );
  UserRule& _rule;
};



}

#endif
