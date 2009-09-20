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
#ifndef PETABRICKSMATRIXIO_H
#define PETABRICKSMATRIXIO_H

#include "matrix.h"

#include "common/jassert.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define MATRIX_ELEMENT_T double
# define MAX_DIMENSIONS   64
#endif

namespace petabricks {

/**
 * Struct for holding outputs of matrixreader.lpp, used for matrixread<->MatrixIO com
 */
struct MatrixReaderScratch {
  ///
  /// Storage allocated to matrix being read
  MatrixStoragePtr storage;
  ///
  /// Pointer to first unread cell of this->storage
  ElementT* buf;
  ///
  /// Remaining space in this->buf, should be zero on completion
  int remaining;
  ///
  /// Number of dimensions
  int dimensions;
  ///
  /// Size of each dimension
  int sizes[MAX_DIMENSIONS];
};


/**
 * A thin wrapper around matrixreader
 */
class MatrixIO{
public:
  ///
  /// Constructor
  MatrixIO(FILE* file = stdin);

  ///
  /// Constructor (opens the given filename)
  MatrixIO(const char* filename, const char* mode);

  ///
  /// Read a D-dimensional matrix from _fd
  template<int D>
  MatrixRegion<D, MATRIX_ELEMENT_T> read(){
    MatrixReaderScratch o;
    _read(o);
    JASSERT(o.dimensions==D)(o.dimensions)(D)
      .Text("Unexpected number of dimensions in input matrix");
    MatrixStorage::IndexT sizes[D];
    for(int i=0; i<D; ++i) sizes[i]=o.sizes[i];
    return MatrixRegion<D, MATRIX_ELEMENT_T>(o.storage, o.storage->data(), sizes);
  }

  ///
  /// Write a given matrix to _fd
  template<int D, typename T>
  void write(MatrixRegion<D,T> m);

  MatrixRegion0D read0D(){ return read<0>(); }
  MatrixRegion1D read1D(){ return read<1>(); }
  MatrixRegion2D read2D(){ return read<2>(); }
  MatrixRegion3D read3D(){ return read<3>(); }
  MatrixRegion4D read4D(){ return read<4>(); }
  MatrixRegion5D read5D(){ return read<5>(); }
  MatrixRegion6D read6D(){ return read<6>(); }
  MatrixRegion7D read7D(){ return read<7>(); }
  MatrixRegion8D read8D(){ return read<8>(); }
  MatrixRegion9D read9D(){ return read<9>(); }
protected:
  void _read(MatrixReaderScratch&);

private:
  FILE* _fd;
};

}

///
/// Write a given matrix to _fd
template<int D, typename T>
inline void petabricks::MatrixIO::write(MatrixRegion<D,T> m){
  if(_fd==stdin) _fd=stdout;
  fprintf(_fd,"SIZE");
  for(int i=0; i<D; ++i)
    fprintf(_fd," %d",m.size(i));
  fprintf(_fd,"\n");
  MatrixStorage::IndexT coord[D];
  memset(coord, 0, sizeof coord);
  if(D>0){
    while(coord[D-1] < m.size(D-1)){
      fprintf(_fd,"%4.8g ", (double) m.cell(coord));
      //get next coord
      coord[0]++;
      for(int i=0; i<D-1; ++i){
        if(coord[i] >= m.size(i)){
          coord[i]=0;
          coord[i+1]++;
          fprintf(_fd,"\n");
        }else
          break;
      }
    }
  }else{ //0D case
    fprintf(_fd,"%4.8g", (double) m.cell(coord));
  }
  fprintf(_fd,"\n");
  fflush(_fd);
}

#endif
