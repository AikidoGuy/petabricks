#ifndef PETABRICKSREGIONHANDLER_H
#define PETABRICKSREGIONHANDLER_H

#include "common/jassert.h"
#include "common/jrefcounted.h"
#include "regiondatai.h"
#include "remotehost.h"

#include <map>

namespace petabricks {
  using namespace petabricks::RegionDataRemoteMessage;

  class RegionHandler;
  typedef jalib::JRef<RegionHandler> RegionHandlerPtr;

  class RegionHandler : public jalib::JRefCounted {
  private:
    RegionDataIPtr _regionData;
    jalib::JMutex _regionDataMux;
    int _D;

  public:
    RegionHandler(const int dimensions);
    RegionHandler(const int dimensions, const IndexT* size, const bool alloc);
    RegionHandler(const RegionDataIPtr regionData);
    //RegionHandler(const EncodedPtr remoteObjPtr);

    ElementT readCell(const IndexT* coord);
    void writeCell(const IndexT* coord, ElementT value);
    void invalidateCache();
    void randomize();

    int allocData();
    int allocData(const IndexT* size);
    int allocDataLocal(const IndexT* size);

    RegionDataIPtr getRegionData();
    void updateRegionData(RegionDataIPtr regionData);

    DataHostPidList hosts(const IndexT* begin, const IndexT* end) const;
    RemoteHostPtr host();

    int dimensions() const;
    const IndexT* size() const;
    RegionDataType type() const;

    // Migration
    void updateHandlerChain();
    RemoteRegionHandler remoteRegionHandler() const;
    bool isHandlerChainUpdated(); // for testing

    // Copy MatrixStorage
    void copyToScratchMatrixStorage(CopyToMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata=0, const IndexT* scratchStorageSize=0);
    void copyFromScratchMatrixStorage(CopyFromMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata=0, const IndexT* scratchStorageSize=0);

    // RegionDataSplit
    void splitData(int dimensions, IndexT* sizes, IndexT* splitSize);
    void createDataPart(int partIndex, RemoteHostPtr host);

    // Process Remote Messages
    void processReadCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processWriteCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processReadCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processWriteCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processGetHostListMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processCopyToMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processCopyFromMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processAllocDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processRandomizeDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processUpdateHandlerChainMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
  };

  typedef std::map<EncodedPtr, RegionHandlerPtr> LocalRegionHandlerMap;

  class RegionHandlerDB {
  public:
    RegionHandlerDB() {}

    static RegionHandlerDB& instance();

    RegionHandlerPtr getLocalRegionHandler(const HostPid& hostPid, const EncodedPtr remoteHandler, const int dimensions, const IndexT* size);

  private:
    jalib::JMutex _mapMux;
    std::map<HostPid, LocalRegionHandlerMap> _map;
    std::map<HostPid, jalib::JMutex*> _localMapMux;
  };
}

#endif
