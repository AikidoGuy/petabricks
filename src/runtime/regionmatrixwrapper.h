#ifndef PETABRICKSREGIONMATRIXWRAPPER_H
#define PETABRICKSREGIONMATRIXWRAPPER_H

#include "petabricksruntime.h"
#include "regionmatrix.h"

namespace petabricks {

  template< int D, typename ElementT>
  class RegionMatrixWrapper : public RegionMatrix {
  public:
    RegionMatrixWrapper() : RegionMatrix(D) {}
    RegionMatrixWrapper(IndexT* size) : RegionMatrix(D, size) {}
 
    RegionMatrixWrapper(ElementT* data, IndexT* size) : RegionMatrix(D, size) {
      IndexT coord[D];
      memset(coord, 0, sizeof coord);
      this->acquireRegionData();
      _regionData->allocData();

      IndexT i = 0;
      do {
	this->writeCell(coord, data[i]);
	i++;
      } while (this->incCoord(coord) >= 0);
      this->releaseRegionData();
    }

    RegionMatrixWrapper(const RegionMatrix& that) : RegionMatrix(that) {} 
    
    static RegionMatrixWrapper allocate(IndexT* size) {
      RegionMatrixWrapper region = RegionMatrixWrapper<D, ElementT>(size);
      region.allocData();
      return region;
    }

    static RegionMatrixWrapper allocate(IndexT x, ...) {
      IndexT c1[D];
      va_list ap;
      va_start(ap, x);
      c1[0]=x;
      for(int i=1; i<D; ++i) c1[i]=va_arg(ap, IndexT);
      va_end(ap);
      return allocate(c1);
    }

    bool isSize(const IndexT size[D]) const{
      for(int i=0; i<D; ++i){
	if(this->size()[i] != size[i]){
	  return false;
	}
      }
      return true;
    }

    bool isSize(IndexT x, ...) const{
      IndexT c1[D];
      va_list ap;
      va_start(ap, x);
      c1[0]=x;
      for(int i=1; i<D; ++i) c1[i]=va_arg(ap, IndexT);
      va_end(ap);
      return isSize(c1);
    }

    ///
    /// Number of elements in this region
    ssize_t count() const {
      ssize_t s=1;
      for(int i=0; i<D; ++i)
	s*=this->size()[i];
      return s;
    }

    RegionMatrixWrapper region(const IndexT c1[D], const IndexT c2[D]) const{
      IndexT newSizes[D];
      for(int i=0; i<D; ++i){
        #ifdef DEBUG
	JASSERT(c1[i]<=c2[i])(c1[i])(c2[i])
	  .Text("region has negative size");
	JASSERT(c2[i]<=size(i))(c2[i])(size(i))
	  .Text("region goes out of bounds");
        #endif
	newSizes[i]=c2[i]-c1[i];
      }
      return RegionMatrixWrapper(this->splitRegion(c1, newSizes));
    }

    RegionMatrixWrapper region(IndexT x, ...) const{
      IndexT c1[D], c2[D];
      va_list ap;
      va_start(ap, x);
      c1[0]=x;
      for(int i=1; i<D; ++i) c1[i]=va_arg(ap, IndexT);
      for(int i=0; i<D; ++i) c2[i]=va_arg(ap, IndexT);
      va_end(ap);
      return region(c1,c2);
    }

    ElementT rand(){
      return PetabricksRuntime::randDouble(-2147483648, 2147483648);
    }

    void randomize() {
      IndexT coord[D];
      memset(coord, 0, sizeof coord);
      this->acquireRegionData();      
      do {
	this->writeCell(coord, this->rand());
      } while (this->incCoord(coord) >= 0);
      this->releaseRegionData();
    }

    void hash(jalib::HashGenerator& gen) {
      IndexT coord[D];
      memset(coord, 0, sizeof coord);
      this->acquireRegionData();      
      do {
	ElementT v = this->readCell(coord);
	gen.update(&v, sizeof(ElementT));
      } while (this->incCoord(coord) >= 0);
      this->releaseRegionData();
    }
  };


  typedef RegionMatrixWrapper<0, double> RegionMatrix0D;
  typedef RegionMatrixWrapper<1, double> RegionMatrix1D;
  typedef RegionMatrixWrapper<2, double> RegionMatrix2D;
  typedef RegionMatrixWrapper<3, double> RegionMatrix3D;
  
  typedef RegionMatrix0D ConstRegionMatrix0D;
  typedef RegionMatrix1D ConstRegionMatrix1D;
  typedef RegionMatrix2D ConstRegionMatrix2D;
  typedef RegionMatrix3D ConstRegionMatrix3D;

}

#endif
