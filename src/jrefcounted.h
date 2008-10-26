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
#ifndef JALIBJREFCOUNTED_H
#define JALIBJREFCOUNTED_H

#include "jassert.h"
#include "jasm.h"

#include <vector>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

namespace jalib {
/**
 * Reference to a JRefCounted
 */
template < typename T> class JRef{
public:
  static const JRef& null() { static JRef t; return t; }

  //constructors
  JRef(T* o = NULL) : _obj(o) { inc(); }
  JRef(const JRef& p) : _obj(p._obj) { inc(); }

  //destructors
  ~JRef() { dec(); }

  //assignment
  JRef& operator= (const JRef& p){
    p.inc(); //must go first for self assignment
    dec();
    _obj = p._obj;
    return *this;
  }

  //accessor
  T* operator->() const {
    check(); 
    return _obj; 
  }

  //accessor
  T& operator*()  const {
    check(); 
    return *_obj; 
  }

   T* asPtr() { return _obj; }
   const T* asPtr() const { return _obj; }

  //is valid?
  operator bool() const { 
    return _obj!=NULL; 
  }
 
  //compare
  bool operator == (const JRef& that) const { 
    return that._obj == _obj;
  }
  bool operator < (const JRef& that) const { 
    return that._obj < _obj;
  }

  operator const T& () const { check(); return *_obj; }
  operator T& ()             { check(); return *_obj; }
private: //helpers:
  void inc() const { if(_obj!=NULL) _obj->incRefCount(); }
  void dec() const { if(_obj!=NULL) _obj->decRefCount(); }
#ifdef DEBUG
  void check() const { JASSERT(_obj!=NULL).Text("Would have dereferenced null pointer."); }
#else
  void check() const {}
#endif
private:
  mutable T* _obj;
};

/**
 * Base class for ref counted objects
 */
class JRefCounted{
protected:
  JRefCounted()                   : _refCount(0) {}
  JRefCounted(const JRefCounted&) : _refCount(0) {}
  virtual ~JRefCounted(){}
public:
  void incRefCount() const{ 
    atomicAdd<1> (&_refCount); 
  }
  void decRefCount() const{ 
    if(atomicAdd<-1>(&_refCount)==0)
      delete this;
  }
private:
  mutable volatile long _refCount;
};

/**
 * A pool of references
 */
class JRefPool {
public:
  template < typename T >  T* add(T* t) { 
    _pool.push_back(JRef<JRefCounted>(t)); 
    return t; 
  }
  void clear() { return _pool.clear(); }
private:
  std::vector< JRef< JRefCounted > > _pool;
};

}

template < typename T >
std::ostream& operator<< (std::ostream& o, const jalib::JRef<T>& ptr){
    return ptr.operator bool() 
         ? o << ptr.operator *() 
         : o << "(null)";
}



#endif
