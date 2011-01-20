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

#include "matrixio.h"
#include "regiondataraw.h"

using namespace petabricks;

PetabricksRuntime::Main* petabricksMainTransform(){
  return NULL;
}
PetabricksRuntime::Main* petabricksFindTransform(const std::string& ){
  return NULL;
}

int main(int argc, const char** argv){
  char* filename1 = "testdata/Helmholtz3DZeros";
  char* filename2 = "testdata/Helmholtz3DB1";

  IndexT m0[] = {0,0,0};
  IndexT m1[] = {1,1,1};
  IndexT m123[] = {1,2,3};
  IndexT m2[] = {2,2,2};
  IndexT m3[] = {3,3,3};

  MatrixIO* matrixio = new MatrixIO(filename2, "r");
  MatrixReaderScratch o = matrixio->readToMatrixReaderScratch();

  RegionDataI<3>::RegionDataIPtr regiondata = new RegionDataRaw<3>(o.sizes, o.storage->data());

  regiondata->print();

  printf("before %4.8g\n", regiondata->readCell(m0));
  regiondata->writeCell(m0, 5);
  printf("after %4.8g\n", regiondata->readCell(m0));

  printf("completed\n");
}
