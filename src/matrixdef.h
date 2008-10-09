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
#ifndef HECURAMATRIXDEF_H
#define HECURAMATRIXDEF_H

#include "jrefcounted.h"
#include "jprintable.h"
#include "formula.h"
#include "jconvert.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vector>
#include <map>
#include <string>

namespace hecura {
class CodeGenerator;
class Transform;
class MatrixDef;
class FreeVars;
typedef jalib::JRef<MatrixDef> MatrixDefPtr;
class MatrixDefList : public std::vector<MatrixDefPtr> , public jalib::JRefCounted {};
class MatrixDefMap : public std::map<std::string, MatrixDefPtr> , public jalib::JRefCounted {};

/**
 * Symbolically defines the size of an input/output matrix
 */
class MatrixDef : public jalib::JRefCounted, public jalib::JPrintable {
public:
  static const MatrixDef& oneD(){
    FormulaList l;
    l.push_back(FormulaInteger::one());
    static MatrixDef tmp("_oneD",l,l);
    return tmp;
  }

  ///
  /// Constructor
  MatrixDef(const char* name, const FormulaList& version, const FormulaList& size);
  
  void print(std::ostream& o) const;

  void initialize(Transform&);
  
  const std::string& name() const { return _name; }

  size_t numDimensions() const { return _size.size(); }
  
  FormulaPtr getSizeOfDimension( size_t n ) const { 
    JASSERT(n < numDimensions())(n)(numDimensions());
    return _size[n]; 
  }

  FormulaPtr width() const  { return getSizeOfDimension(0); }
  FormulaPtr height() const { return getSizeOfDimension(1); }

  FormulaPtr getMaxValueInDimension(size_t n) const {
    return new FormulaSubtract(getSizeOfDimension(n), FormulaInteger::one());
  }

  void exportAssumptions();

  void exportConstants(Transform&);

  std::string matrixTypeName() const{
    #ifdef SHORT_TYPE_NAMES
    return "MatrixRegion"+jalib::XToString(numDimensions())+"D";
    #else
    return "hecura::MatrixRegion<"+jalib::XToString(numDimensions())+">";
    #endif
  }

  std::string constMatrixTypeName() const{
    #ifdef SHORT_TYPE_NAMES
    return "ConstMatrixRegion"+jalib::XToString(numDimensions())+"D";
    #else
    return "hecura::MatrixRegion<"+jalib::XToString(numDimensions())+", const MATRIX_ELEMENT_T>";
    #endif
  }

  void argDeclRW(std::vector<std::string>& args) const;
  void argDeclRO(std::vector<std::string>& args) const;
  void genAllocTmpCode(CodeGenerator& o);
  void generateCodeSimple(CodeGenerator& o);

  void extractDefines(FreeVars& defined, CodeGenerator& o);
  void verifyDefines(CodeGenerator& o);
  void allocateTemporary(CodeGenerator& o);
private:
  std::string _name;
  FormulaList _version;
  FormulaList _size;
};

}

#endif
