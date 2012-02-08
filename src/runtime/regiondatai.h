#ifndef PETABRICKSREGIONDATAI_H
#define PETABRICKSREGIONDATAI_H

#include "common/jrefcounted.h"
#include "iregionreplyproxy.h"
#include "matrixstorage.h"
#include "regiondataremotemessages.h"

namespace petabricks {

  using namespace petabricks::RegionDataRemoteMessage;

  //
  // RegiondataI
  //

  class RegionDataI;
  typedef jalib::JRef<RegionDataI> RegionDataIPtr;

  class RegionDataI : public jalib::JRefCounted {
  protected:
    int _D;
    RegionDataType _type;

    // _size is the size of this part, not the entire region
    IndexT _size[MAX_DIMENSIONS];

  public:
    virtual int allocData() = 0;

    virtual ElementT readCell(const IndexT* coord) const = 0;
    virtual void writeCell(const IndexT* coord, ElementT value) = 0;
    virtual void invalidateCache() {}

    virtual MatrixStoragePtr storage() const {
      JASSERT(false)(_type).Text("This should not be called.");
      return NULL;
    }

    virtual void copyToScratchMatrixStorage(CopyToMatrixStorageMessage* /*origMetadata*/, size_t /*len*/, MatrixStoragePtr /*scratchStorage*/, RegionMatrixMetadata* /*scratchMetadata*/=0) const {
      UNIMPLEMENTED();
    }

    virtual void copyFromScratchMatrixStorage(CopyFromMatrixStorageMessage* /*metadata*/, size_t /*len*/) const {
      UNIMPLEMENTED();
    }

    virtual void setStorage(MatrixStoragePtr /*storage*/) {
      JASSERT(false)(_type).Text("This should not be called.");
    }

    virtual void randomize() {
      this->storage()->randomize();
    }

    // for toLocalRegion
    virtual ElementT& value0D(const IndexT* /*coord*/) const {
      JASSERT(false)(_type).Text("This should not be called.");
      throw;
    }

    int dimensions();
    IndexT* size();

    RegionDataType type() const {
      return _type;
    }

    virtual DataHostPidList hosts(IndexT* begin, IndexT* end) = 0;
    virtual RemoteHostPtr host() = 0;

    // Process Remote Messages
    virtual void processReadCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processWriteCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processReadCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processWriteCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processGetHostListMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processCopyFromMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processCopyToMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processAllocDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processRandomizeDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    virtual void processUpdateHandlerChainMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller, RegionDataIPtr regionDataPtr);

    // for tests
  private:
    int incCoord(IndexT* coord);
  public:
    virtual void print();
  };
}

#endif
