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
#include "formula.h"
#include "maximawrapper.h"


hecura::FormulaList::FormulaList(const FormulaList& that) 
  : std::vector<FormulaPtr>(that) 
{} 

hecura::FormulaList::FormulaList() {}

static const hecura::FreeVarsPtr& theNullFreeVarsList(){
  static hecura::FreeVarsPtr inst = new hecura::FreeVars();
  return inst;
}

hecura::FormulaVariable::FormulaVariable(const char* name) 
  : Formula(theNullFreeVarsList())
  , _name(name) 
{
  FreeVars* fv;
  _freeVars = fv = new FreeVars();
  fv->insert(_name);
}

hecura::FormulaVariable::FormulaVariable(const std::string& name) 
  : Formula(theNullFreeVarsList())
  , _name(name) 
{
  FreeVars* fv;
  _freeVars = fv = new FreeVars();
  fv->insert(_name);
}

void hecura::FormulaVariable::print(std::ostream& o) const { 
  o << _name; 
}

template < typename T >
hecura::FormulaLiteral<T>::FormulaLiteral(T v) 
  : Formula(theNullFreeVarsList())
  , _value(v) 
{}

template < typename T >
void hecura::FormulaLiteral<T>::print(std::ostream& o) const { 
  o << _value; 
}

template < char OP >
hecura::FormulaBinop<OP>::FormulaBinop(const FormulaPtr& left, const FormulaPtr& right)
  : Formula(theNullFreeVarsList())
  , _left(left)
  , _right(right) 
{
  _size = _left->size() + _right->size();
  FreeVars* fv;
  _freeVars = fv = new FreeVars();
  _left->getFreeVariables( *fv );
  _right->getFreeVariables( *fv );
}

template < char OP >
void hecura::FormulaBinop<OP>::print(std::ostream& o) const {
  o << '(' << _left << opStr() << _right << ')';
}

template < char OP >
const char* hecura::FormulaBinop<OP>::opStr() {
  if(OP=='G') return ">=";
  if(OP=='L') return "<=";
  static const char v[] = {OP , 0};
  return v;
}

void hecura::FormulaList::normalize(){
  for(iterator i = begin(); i!=end(); ++i)
    (*i) = MaximaWrapper::instance().normalize( *i );
}

void hecura::FormulaList::print(std::ostream& o) const{
  printStlList(o, begin(), end(), ", ");
} 

hecura::FreeVarsPtr hecura::FormulaList::getFreeVariables() const {
  FreeVarsPtr ret;
  FreeVars* fv;
  ret = fv = new FreeVars();
  for(const_iterator i=begin(); i!=end(); ++i) 
    (*i)->getFreeVariables(*fv);
  return ret;
}

void hecura::FormulaList::makeRelativeTo(const FormulaList& defs){
  for(iterator i=begin(); i!=end(); ++i){
    for(const_iterator d=defs.begin(); d!=defs.end(); ++d){
      *i = MaximaWrapper::instance().subst(*d, *i);
    }
  }
} 

bool hecura::Formula::hasIntersection(const Formula& that) const{
  const FreeVars& a = getFreeVariables();
  const FreeVars& b = that.getFreeVariables();

  // speed optimization
  if(b.size() < a.size()) 
    return that.hasIntersection(*this);

  for(FreeVars::const_iterator i=a.begin(); i!=a.end(); ++i){
    if(b.find(*i) != b.end())
      return true;
  }
  return false;
}

hecura::FormulaPtr hecura::FormulaVariable::mktmp(){
  static volatile long i = 0;
  std::string name = "_tmp" + jalib::XToString(jalib::atomicAdd<1>(&i));
  return new FormulaVariable(name);
}

//force implementations to be generated for templates
template class hecura::FormulaLiteral<int>;
template class hecura::FormulaLiteral<double>;
template class hecura::FormulaLiteral<bool>;
template class hecura::FormulaBinop<'+'>;
template class hecura::FormulaBinop<'-'>;
template class hecura::FormulaBinop<'*'>;
template class hecura::FormulaBinop<'/'>;
template class hecura::FormulaBinop<'='>;
template class hecura::FormulaBinop<'>'>;
template class hecura::FormulaBinop<'G'>;
template class hecura::FormulaBinop<'<'>;
template class hecura::FormulaBinop<'L'>;
