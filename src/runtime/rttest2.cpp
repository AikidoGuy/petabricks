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
#include "petabricks.h"

using namespace petabricks;

PetabricksRuntime::Main* petabricksMainTransform(){
  return NULL;
}
PetabricksRuntime::Main* petabricksFindTransform(const std::string& ){
  return NULL;
}


int main(int argc, const char** argv){
  MatrixIO* matrixio = new MatrixIO(argv[1], "r");
  RegionIPtr region = matrixio->readToRegionI();

  IndexT m0[] = {0,0,0};
  IndexT m1[] = {1,1,1};
  IndexT m123[] = {1,2,3};
  IndexT m2[] = {2,2,2};
  IndexT m3[] = {3,3,3};

  RegionIPtr split3 = region->splitRegion(m123, m3);
  RegionIPtr split2 = split3->splitRegion(m1, m2);
  split3->print();
  split2->print();

  RegionIPtr slice1 = split2->sliceRegion(2, 0);
  slice1->print();

  RegionIPtr slice2 = slice1->sliceRegion(1, 1);
  slice2->print();

  return 0;
}

