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
#ifndef PETABRICKSMATRIXSTORAGE_H
#define PETABRICKSMATRIXSTORAGE_H

#include "common/jassert.h"
#include "common/jrefcounted.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_OPENSSL_MD5_H
# include <openssl/md5.h>
#endif
#ifdef HAVE_OPENSSL_SHA_H
# include <openssl/sha.h>
#endif

namespace petabricks {

class MatrixStorage;
typedef jalib::JRef<MatrixStorage> MatrixStoragePtr;

/**
 * The raw data for a Matrix
 */
class MatrixStorage : public jalib::JRefCounted {
public:
  typedef MATRIX_INDEX_T IndexT;
  typedef MATRIX_ELEMENT_T ElementT;
private:
  //no copy constructor
  MatrixStorage(const MatrixStorage&);
public:
  ///
  /// Constructor
  MatrixStorage(size_t n) : _count(n) {
    _data = new ElementT[n];
  }

  ///
  /// Destructor
  ~MatrixStorage(){
    delete [] _data;
  }

  ElementT* data() { return _data; }
  const ElementT* data() const { return _data; }

  size_t count() const { return _count; }

  ///
  /// Fill the matrix with random data
  void randomize();

  ///
  /// generate a single random number
  static MATRIX_ELEMENT_T rand();

  struct HashT {
    unsigned char buf[HASH_LEN];
    friend bool operator==(const HashT& a, const HashT& b){
      return memcmp(a.buf, b.buf, HASH_LEN)==0;
    }
    HashT(){memset(buf, 0, sizeof buf);}
  };
  HashT hash() const { 
    HashT h;
    JASSERT(h.buf == HASH_FN((const unsigned char*)_data, _count*sizeof(ElementT), h.buf));
    return h;
  }
private:
  ElementT* _data;
  size_t _count;
};


/**
 * Capable of storing any type of MatrixRegion plus a hash of its data
 */
class MatrixStorageInfo {
  typedef MatrixStorage::IndexT IndexT;
  typedef MatrixStorage::ElementT ElementT;
  typedef MatrixStorage::HashT HashT;
public:
  const MatrixStoragePtr& storage() const { return _storage; }
  ElementT* base() const { return _storage->data()+_baseOffset; }
  int dimensions() const { return _dimensions; }
  const IndexT* multipliers() const { return _multipliers; }
  const IndexT* sizes() const { return _sizes; }
  const HashT& hash() const { return _hash; }
  ElementT extraVal() const { return _extraVal; }

  void setStorage(const MatrixStoragePtr& s, const ElementT* base);
  void setSizeMultipliers(int dim, const IndexT* mult, const IndexT* siz);
  void setExtraVal(ElementT v=0);
  void computeDataHash();
  void reset();
  void releaseStorage();
  MatrixStorageInfo();
  bool isMetadataMatch(const MatrixStorageInfo& that) const;
  bool isDataMatch(const MatrixStorageInfo& that) const;
private:
  MatrixStoragePtr _storage;
  int     _dimensions;
  ssize_t _baseOffset;
  IndexT  _multipliers[MAX_DIMENSIONS];
  IndexT  _sizes[MAX_DIMENSIONS];
  ElementT _extraVal;
  HashT   _hash;
};




}

#endif

