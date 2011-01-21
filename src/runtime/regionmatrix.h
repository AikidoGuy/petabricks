#ifndef PETABRICKSREGIONMATRIX_H
#define PETABRICKSREGIONMATRIX_H

#include "common/jrefcounted.h"

#include "regiondatai.h"
#include "regionhandler.h"

namespace petabricks {
  class RegionMatrix;
  typedef jalib::JRef<RegionMatrix> RegionMatrixPtr;
  
  class RegionMatrix : public jalib::JRefCounted {  
  protected:
    RegionHandlerPtr _regionHandler;
    
    RegionDataIPtr _regionData;
    
    int _D;
    IndexT* _size;
    IndexT* _splitOffset;
    int _numSliceDimensions;
    int* _sliceDimensions;
    IndexT* _slicePositions;

  public:
    RegionMatrix(RegionDataIPtr regionData);
    RegionMatrix(RegionHandlerPtr handler, int dimensions, IndexT* size,
		 IndexT* splitOffset, int numSliceDimensions,
		 int* sliceDimensions, IndexT* slicePositions);
    ~RegionMatrix();

    RegionMatrixPtr splitRegion(IndexT* offset, IndexT* size);
    RegionMatrixPtr sliceRegion(int d, IndexT pos);

    ElementT readCell(const IndexT* coord);
    void writeCell(const IndexT* coord, ElementT value);

    void acquireRegionData();
    void releaseRegionData();

    // for tests
    int incCoord(IndexT* coord);
    void print();

  private:
    IndexT* getRegionDataCoord(const IndexT* coord_orig);

  };
}

#endif
