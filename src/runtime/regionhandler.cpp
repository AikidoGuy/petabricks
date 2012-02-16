#include "regionhandler.h"

#include "regiondataraw.h"
#include "regiondataremote.h"
#include "regiondatasplit.h"
#include "regionmatrixproxy.h"

using namespace petabricks;
using namespace petabricks::RegionDataRemoteMessage;

RegionHandler::RegionHandler(const int dimensions) {
  _D = dimensions;
}

RegionHandler::RegionHandler(const int dimensions, const IndexT* size, const bool alloc = false) {
  _regionData = new RegionDataRaw(dimensions, size);
  _D = dimensions;
  if (alloc) {
    _regionData->allocData();
  }
}

RegionHandler::RegionHandler(const RegionDataIPtr regionData) {
  _regionData = regionData;
  _D = _regionData->dimensions();
}

ElementT RegionHandler::readCell(const IndexT* coord) {
  return _regionData->readCell(coord);
}

void RegionHandler::writeCell(const IndexT* coord, ElementT value) {
  _regionData->writeCell(coord, value);
}

void RegionHandler::invalidateCache() {
  return _regionData->invalidateCache();
}

void RegionHandler::randomize() {
  _regionData->randomize();
}

int RegionHandler::allocData() {
  return _regionData->allocData();
}

int RegionHandler::allocData(const IndexT* size) {
  if (_regionData) {
    JASSERT(type() == RegionDataTypes::REGIONDATASPLIT);
    _regionData->allocData();
    return 1;
  }

  // Create RegionData
  // TODO: do clever data placement

#ifdef REGIONMATRIX_TEST
  _regionData = new RegionDataRemote(_D, size, RemoteHostDB::instance().host(0));
#else
  // round-robin placement
  static int numHosts = RemoteHostDB::instance().size();
  static int currentIndex = 0;

  int i = (currentIndex + 1) % (numHosts + 1);
  currentIndex = i;

  if (i < numHosts) {
    _regionData = new RegionDataRemote(_D, size, RemoteHostDB::instance().host(i));

  } else if (i == numHosts) {
    _regionData = new RegionDataRaw(_D, size);
    _regionData->allocData();

  } else {
    JASSERT(false)(i)(numHosts);
  }
#endif
  return 1;
}

int RegionHandler::allocDataLocal(const IndexT* size) {
  if (_regionData) {
    JASSERT(type() == RegionDataTypes::REGIONDATASPLIT);
    _regionData->allocData();
    return 1;
  }

  // Create local data
  _regionData = new RegionDataRaw(_D, size);
  _regionData->allocData();

  return 1;
}

RegionDataIPtr RegionHandler::getRegionData() {
  return _regionData;
}

void RegionHandler::updateRegionData(RegionDataIPtr regionData) {
  _regionDataMux.lock();
  _regionData = regionData;
  _regionDataMux.unlock();
}

DataHostPidList RegionHandler::hosts(const IndexT* begin, const IndexT* end) const {
  return _regionData->hosts(begin, end);
}

RemoteHostPtr RegionHandler::dataHost() {
  return _regionData->dataHost();
}

int RegionHandler::dimensions() const {
  return _D;
}

const IndexT* RegionHandler::size() const {
  return _regionData->size();
}

RegionDataType RegionHandler::type() const {
  return _regionData->type();
}

//
// Migration
//

void RegionHandler::updateHandlerChain() {
  if (type() == RegionDataTypes::REGIONDATAREMOTE) {
    RegionDataRemoteMessage::UpdateHandlerChainReplyMessage reply =
      ((RegionDataRemote*)_regionData.asPtr())->updateHandlerChain();
    //JTRACE("done updatehandler")(reply.dataHost)(reply.numHops);

    if (reply.dataHost == HostPid::self()) {
      // Data is in the same process. Update handler to point directly to the data.
      RegionDataI* regionData = reinterpret_cast<RegionDataI*>(reply.encodedPtr);
      updateRegionData(regionData);

    } else if (reply.numHops > 1) {
      // Multiple network hops to data. Create a direct connection to data.
      RegionDataI* newRegionData = new RegionDataRemote(_regionData->dimensions(), _regionData->size(), reply.dataHost, reply.encodedPtr);
      updateRegionData(newRegionData);
    }
  }
}

// For testing.
bool RegionHandler::isHandlerChainUpdated() {
  if (type() == RegionDataTypes::REGIONDATAREMOTE) {
    RegionDataRemoteMessage::UpdateHandlerChainReplyMessage reply =
      ((RegionDataRemote*)_regionData.asPtr())->updateHandlerChain();

    JTRACE("isHandlerChainUpdated")(reply.dataHost)(reply.numHops);
    if (reply.dataHost == HostPid::self()) {
      return false;
    } else if (reply.numHops > 1) {
      return false;
    }
  }
  return true;
}

RemoteRegionHandler RegionHandler::remoteRegionHandler() const {
  if (type() == RegionDataTypes::REGIONDATAREMOTE) {

    return *(((RegionDataRemote*)_regionData.asPtr())->remoteRegionHandler());

  } else {
    RemoteRegionHandler remoteRegionHandler;
    remoteRegionHandler.hostPid = HostPid::self();
    remoteRegionHandler.remoteHandler = reinterpret_cast<EncodedPtr>(this);
    return remoteRegionHandler;
  }
}

void RegionHandler::copyToScratchMatrixStorage(CopyToMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize) {
#ifdef DEBUG
  if (type() == RegionDataTypes::REGIONDATARAW && scratchMetadata == 0) {
    JASSERT(false).Text("This is inefficient. Use _regionData->storage() instead.");
  }
#endif

  RegionDataIPtr newRegionData =
    _regionData->copyToScratchMatrixStorage(origMsg, len, scratchStorage, scratchMetadata, scratchStorageSize);
  if (newRegionData) {
    updateRegionData(newRegionData);
  }
}

void RegionHandler::copyFromScratchMatrixStorage(CopyFromMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize) {
#ifdef DEBUG
  if (type() == RegionDataTypes::REGIONDATARAW && scratchMetadata == 0) {
    JASSERT(false).Text("This is inefficient. Use _regionData->storage() instead.");
  }
#endif

  _regionData->copyFromScratchMatrixStorage(origMsg, len, scratchStorage, scratchMetadata, scratchStorageSize);
}


//
// RegionDataSplit
//

void RegionHandler::splitData(int dimensions, IndexT* sizes, IndexT* splitSize) {
  JASSERT((!_regionData) || type() == RegionDataTypes::REGIONDATARAW);
  RegionDataIPtr newRegionData =
    new RegionDataSplit(dimensions, sizes, splitSize);
  updateRegionData(newRegionData);
}

void RegionHandler::createDataPart(int partIndex, RemoteHostPtr host) {
  JASSERT(type() == RegionDataTypes::REGIONDATASPLIT);
  ((RegionDataSplit*)_regionData.asPtr())->createPart(partIndex, host);
}

// Process Remote Messages
void RegionHandler::processReadCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processReadCellMsg(base, baseLen, caller);
}

void RegionHandler::processWriteCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processWriteCellMsg(base, baseLen, caller);
}

void RegionHandler::processReadCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processReadCellCacheMsg(base, baseLen, caller);
}

void RegionHandler::processWriteCellCacheMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processWriteCellCacheMsg(base, baseLen, caller);
}

void RegionHandler::processGetHostListMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processGetHostListMsg(base, baseLen, caller);
}

void RegionHandler::processCopyFromMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processCopyFromMatrixStorageMsg(base, baseLen, caller);
}

void RegionHandler::processCopyToMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processCopyToMatrixStorageMsg(base, baseLen, caller);
}

void RegionHandler::processAllocDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processAllocDataMsg(base, baseLen, caller);
}

void RegionHandler::processRandomizeDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processRandomizeDataMsg(base, baseLen, caller);
}

void RegionHandler::processUpdateHandlerChainMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  _regionData->processUpdateHandlerChainMsg(base, baseLen, caller, reinterpret_cast<EncodedPtr>(this));
}

//
// RegionHandlerDB
//

RegionHandlerDB& RegionHandlerDB::instance() {
  static RegionHandlerDB db;
  return db;
}

RegionHandlerPtr RegionHandlerDB::getLocalRegionHandler(const HostPid& hostPid, const EncodedPtr remoteHandler, const int dimensions, const IndexT* size) {
  if (hostPid == HostPid::self()) {
    return reinterpret_cast<RegionHandler*>(remoteHandler);
  }

  _mapMux.lock();
  if (_map.count(hostPid) == 0) {
    _map[hostPid] = LocalRegionHandlerMap();
    _localMapMux[hostPid] = new jalib::JMutex();
  }
  LocalRegionHandlerMap& localMap = _map[hostPid];
  jalib::JMutex* localMux = _localMapMux[hostPid];
  _mapMux.unlock();

  localMux->lock();
  if (localMap.count(remoteHandler) == 0) {
    // create a new one
    RegionDataIPtr regionData = new RegionDataRemote(dimensions, size, hostPid, remoteHandler);
    localMap[remoteHandler] = new RegionHandler(regionData);
  }

  RegionHandlerPtr localHandler = localMap[remoteHandler];
  localMux->unlock();

  return localHandler;
}

