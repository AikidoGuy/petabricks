#include "regiondataremote.h"

#include "regiondatasplit.h"
#include "regionmatrixproxy.h"
#include "workerthread.h"

using namespace petabricks;
using namespace petabricks::RegionDataRemoteMessage;

RegionDataRemote::RegionDataRemote(const int dimensions, const IndexT* size, RemoteHostPtr host) {
  init(dimensions, size);

  // InitialMsg
  size_t size_sz = _D * sizeof(IndexT);
  size_t msg_len = sizeof(CreateRegionDataInitialMessage) + size_sz;

  char buf[msg_len];
  CreateRegionDataInitialMessage* msg = (CreateRegionDataInitialMessage*)buf;
  msg->type = MessageTypes::CREATEREMOTEREGIONDATA;
  msg->dimensions = _D;
  memcpy(msg->size, size, size_sz);

  _isRemoteRegionHandlerReady = false;
  _remoteRegionHandler.remoteHandler = 0;

  host->createRemoteObject(this, &RegionMatrixProxy::genRemote, buf, msg_len);
}

RegionDataRemote::RegionDataRemote(const int dimensions, const IndexT* size, const HostPid& hostPid, const EncodedPtr remoteHandler, bool isDataSplit) {
  init(dimensions, size);

  // Defer the initialization of _remoteObject
  _remoteRegionHandler.hostPid = hostPid;
  _remoteRegionHandler.remoteHandler = remoteHandler;
  _isDataSplit = isDataSplit;
  _isRemoteRegionHandlerReady = true;
}

void RegionDataRemote::init(const int dimensions, const IndexT* size) {
  _D = dimensions;
  _type = RegionDataTypes::REGIONDATAREMOTE;

  memcpy(_size, size, sizeof(IndexT) * _D);

  _isDataSplit = false;
}

void RegionDataRemote::createRemoteObject() const {
  if (this->isInitiator()) {
    return;
  }
  int len = sizeof(EncodedPtrInitialMessage);
  EncodedPtrInitialMessage msg;
  msg.type = MessageTypes::INITWITHREGIONHANDLER;
  msg.encodedPtr = _remoteRegionHandler.remoteHandler;
  RemoteHostPtr host = RemoteHostDB::instance().host(_remoteRegionHandler.hostPid);
  // Create RemoteObject takes `const RemoteObjectPtr`
  host->createRemoteObject(const_cast<RegionDataRemote*>(this), &RegionMatrixProxy::genRemote, &msg, len);
}

int RegionDataRemote::allocData() {
  void* data;
  size_t len;
  int type;
  this->fetchData(0, MessageTypes::ALLOCDATA, 0, &data, &len, &type);
  BaseMessageHeader* base = (BaseMessageHeader*)data;
  len = len - base->contentOffset;
  AllocDataReplyMessage* reply = (AllocDataReplyMessage*) base->content();

  ElementT result = reply->result;
  free(data);
  return result;
}

void RegionDataRemote::allocDataNonBlock(jalib::AtomicT* responseCounter) {
  this->fetchDataNonBlock(0, MessageTypes::ALLOCDATA, 0, responseCounter);
}

void RegionDataRemote::randomize() {
  void* data;
  size_t len;
  int type;
  this->fetchData(0, MessageTypes::RANDOMIZEDATA, 0, &data, &len, &type);
  free(data);
}

void RegionDataRemote::randomizeNonBlock(jalib::AtomicT* responseCounter) {
  this->fetchDataNonBlock(0, MessageTypes::RANDOMIZEDATA, 0, responseCounter);
}

const RemoteRegionHandler* RegionDataRemote::remoteRegionHandler() const {
  if (!_isRemoteRegionHandlerReady) {
    remoteObject()->waitUntilCreated();
  }
  return &_remoteRegionHandler;
}


IRegionCachePtr RegionDataRemote::cacheGenerator() const {
  IndexT multipliers[_D];
  multipliers[0] = 1;
  for (int i = 1; i < _D; i++) {
    multipliers[i] = multipliers[i - 1] * _size[i - 1];
  }

  return new RegionDataRemoteCache(this, _D, multipliers);
}

IRegionCachePtr RegionDataRemote::cache() const {
#ifdef DISTRIBUTED_CACHE
  if (WorkerThread::self()) {
    return WorkerThread::self()->cache()->get(this);
  } else {
    // in listening loop
    return NULL;
  }
#else
  return NULL;
#endif
}

void RegionDataRemote::invalidateCache() {
#ifdef DISTRIBUTED_CACHE
  if (cache()) {
    cache()->invalidate();
  }
#endif
}

ElementT RegionDataRemote::readCell(const IndexT* coord) const {
  JTRACE("remote read");
#ifdef DISTRIBUTED_CACHE
  if (cache()) {
    return cache()->readCell(coord);
  } else {
    return readNoCache(coord);
  }
#else
  return readNoCache(coord);
#endif
}

ElementT RegionDataRemote::readNoCache(const IndexT* coord) const {
  size_t coord_sz = _D * sizeof(IndexT);
  size_t msg_len = sizeof(ReadCellMessage) + coord_sz;

  char buf[msg_len];
  ReadCellMessage* msg = (ReadCellMessage*)buf;
  memcpy(msg->coord, coord, coord_sz);

  void* data;
  size_t len;
  int type;
  this->fetchData(buf, MessageTypes::READCELL, msg_len, &data, &len, &type);
  BaseMessageHeader* base = (BaseMessageHeader*)data;
  len = len - base->contentOffset;

  ReadCellReplyMessage* reply = (ReadCellReplyMessage*) base->content();
  ElementT value = reply->value;
  free(data);
  return value;
}

void RegionDataRemote::readByCache(void* request, size_t request_len, void* reply, size_t &/*reply_len*/) const {
  void* data;
  size_t len;
  int type;
  this->fetchData(request, MessageTypes::READCELLCACHE, request_len, &data, &len, &type);
  BaseMessageHeader* base = (BaseMessageHeader*)data;
  len = len - base->contentOffset;

  ReadCellCacheReplyMessage* r = (ReadCellCacheReplyMessage*) base->content();

  RegionDataRemoteCacheLine* cacheLine = (RegionDataRemoteCacheLine*)reply;
  cacheLine->start = r->start;
  cacheLine->end = r->end;
  memcpy(cacheLine->base, r->values, sizeof(ElementT) * (r->end + 1));
  free(data);
}

void RegionDataRemote::writeCell(const IndexT* coord, ElementT value) {
  JTRACE("remote write");
#ifdef DISTRIBUTED_CACHE
  if (cache()) {
    cache()->writeCell(coord, value);
  } else {
    writeNoCache(coord, value);
  }
#else
  writeNoCache(coord, value);
#endif
}

void RegionDataRemote::writeNoCache(const IndexT* coord, ElementT value) {
  size_t coord_sz = _D * sizeof(IndexT);
  size_t msg_len = sizeof(WriteCellMessage) + coord_sz;

  char buf[msg_len];
  WriteCellMessage* msg = (WriteCellMessage*)buf;
  msg->value = value;
  memcpy(msg->coord, coord, coord_sz);

  //JTRACE("write")(_D)(sizeof(IndexT))(coord_sz)(msg_len);

  void* data;
  size_t len;
  int type;
  this->fetchData(buf, MessageTypes::WRITECELL, msg_len, &data, &len, &type);
  free(data);
}

void RegionDataRemote::writeByCache(const IndexT* coord, ElementT value) const {
  size_t coord_sz = _D * sizeof(IndexT);
  size_t msg_len = sizeof(WriteCellMessage) + coord_sz;

  char buf[msg_len];
  WriteCellMessage* msg = (WriteCellMessage*)buf;
  msg->value = value;
  memcpy(msg->coord, coord, coord_sz);

  void* data;
  size_t len;
  int type;
  this->fetchData(msg, MessageTypes::WRITECELL, msg_len, &data, &len, &type);
  free(data);
}

RegionDataIPtr RegionDataRemote::copyToScratchMatrixStorage(CopyToMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize, RegionDataI** newScratchRegionData) {
  if (isDataSplit()) {
    RegionDataI* rv = NULL;
    if (this->copyRegionDataSplit()) {
      rv = _localRegionDataSplit.asPtr();
    }
    _localRegionDataSplit->copyToScratchMatrixStorage(origMsg, len, scratchStorage, scratchMetadata, scratchStorageSize, newScratchRegionData);
    return rv;
  }

  void* data;
  size_t replyLen;
  int type;
  this->fetchData(origMsg, MessageTypes::TOSCRATCHSTORAGE, len, &data, &replyLen, &type);
  BaseMessageHeader* base = (BaseMessageHeader*)data;
  len = len - base->contentOffset;

  JASSERT(type == MessageTypes::TOSCRATCHSTORAGE);
  CopyToMatrixStorageReplyMessage* reply = (CopyToMatrixStorageReplyMessage*) base->content();
  JTRACE("copy to scratch")(reply->count);
  if (reply->count == scratchStorage->count()) {
    memcpy(scratchStorage->data(), reply->storage, sizeof(ElementT) * reply->count);

  } else {
    RegionMatrixMetadata* origMetadata = &(origMsg->srcMetadata);
    int d = origMetadata->dimensions;
    IndexT* size = origMetadata->size();

    int n = 0;
    IndexT coord[d];
    memset(coord, 0, sizeof coord);
    IndexT multipliers[d];
    sizeToMultipliers(d, scratchStorageSize, multipliers);
    do {
      IndexT scratchIndex = toRegionDataIndex(d, coord, scratchMetadata->numSliceDimensions, scratchMetadata->splitOffset, scratchMetadata->sliceDimensions(), scratchMetadata->slicePositions(), multipliers);
      scratchStorage->data()[scratchIndex] = reply->storage[n];
      ++n;
    } while(incCoord(d, size, coord) >= 0);
  }

  free(data);
  return NULL;
}

void RegionDataRemote::copyFromScratchMatrixStorage(CopyFromMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize) {
  if (isDataSplit()) {
    _localRegionDataSplit->copyFromScratchMatrixStorage(origMsg, len, scratchStorage, scratchMetadata, scratchStorageSize);
    return;
  }

  RegionMatrixMetadata* origMetadata = &(origMsg->srcMetadata);
  int d = origMetadata->dimensions;
  IndexT* size = origMetadata->size();

  size_t storageCount = 1;
  for (int i = 0; i < d; ++i) {
    storageCount *= size[i];
  }

  // Copy storage.
  if (storageCount == scratchStorage->count()) {
    // Special case: copy the entire storage
    memcpy(origMsg->storage(), scratchStorage->data(), sizeof(ElementT) * storageCount);

  } else {
    int n = 0;
    IndexT coord[d];
    memset(coord, 0, sizeof coord);
    IndexT multipliers[d];
    sizeToMultipliers(d, scratchStorageSize, multipliers);
    do {
      IndexT scratchIndex = toRegionDataIndex(d, coord, scratchMetadata->numSliceDimensions, scratchMetadata->splitOffset, scratchMetadata->sliceDimensions(), scratchMetadata->slicePositions(), multipliers);
      origMsg->storage()[n] = scratchStorage->data()[scratchIndex];
      ++n;
    } while(incCoord(d, size, coord) >= 0);
  }

  JTRACE("copy from scratch")(storageCount);

  size_t msgLen =  RegionMatrixMetadata::len(origMetadata->dimensions, origMetadata->numSliceDimensions) + (sizeof(ElementT) * storageCount);
  JASSERT(msgLen <= len);

  void* data;
  size_t replyLen;
  int type;
  this->fetchData(origMsg, MessageTypes::FROMSCRATCHSTORAGE, msgLen, &data, &replyLen, &type);
  free(data);
}

RegionDataIPtr RegionDataRemote::hosts(const IndexT* begin, const IndexT* end, DataHostPidList& list) {
  if (!_isDataSplit) {
    double count = 1;
    for(int i = 0; i < _D; i++){
      count *= (end[i] - begin[i]);
    }
    DataHostPidListItem item = {remoteRegionHandler()->hostPid, count};
    list.push_back(item);
    return NULL;
  }

  RegionDataI* rv = NULL;
  if (this->copyRegionDataSplit()) {
    rv = _localRegionDataSplit.asPtr();
  }

  _localRegionDataSplit->hosts(begin, end, list);
  return rv;
}

RemoteHostPtr RegionDataRemote::dataHost() {
  return remoteObject()->host();
}

bool RegionDataRemote::isDataSplit() const {
  return _isDataSplit;
}

UpdateHandlerChainReplyMessage RegionDataRemote::updateHandlerChain(UpdateHandlerChainMessage& msg) {
  msg.numHops += 1;

  void* data;
  size_t len;
  int type;
  this->fetchData(&msg, MessageTypes::UPDATEHANDLERCHAIN, sizeof(UpdateHandlerChainMessage), &data, &len, &type);
  BaseMessageHeader* base = (BaseMessageHeader*)data;
  len = len - base->contentOffset;

  UpdateHandlerChainReplyMessage* _reply = (UpdateHandlerChainReplyMessage*) base->content();

  UpdateHandlerChainReplyMessage reply = *_reply;
  free(data);
  return reply;
}

UpdateHandlerChainReplyMessage RegionDataRemote::updateHandlerChain() {
  UpdateHandlerChainMessage msg;
  msg.requester = HostPid::self();
  msg.numHops = 0;
  return this->updateHandlerChain(msg);
}

void RegionDataRemote::fetchData(const void* msg, MessageType type, size_t len, void** responseData, size_t* responseLen, int* responseType) const {
  *responseData = 0;
  *responseLen = 0;
  *responseType = 0;

  GeneralMessageHeader header;
  header.isForwardMessage = false;
  header.type = type;
  header.contentOffset = sizeof(GeneralMessageHeader);
  header.responseData = reinterpret_cast<EncodedPtr>(responseData);
  header.responseLen = reinterpret_cast<EncodedPtr>(responseLen);
  header.responseType = reinterpret_cast<EncodedPtr>(responseType);
  header.responseCounter = 0;

  remoteObject()->send(&header, sizeof(GeneralMessageHeader), msg, len, type);

  JLOCKSCOPE(*this);
  // wait for the data
  while (*responseData == 0 || *responseLen == 0) {
    this->waitMsgMu();
  }
}

void RegionDataRemote::fetchDataNonBlock(const void* msg, MessageType type, size_t len, jalib::AtomicT* responseCounter) const {
  jalib::atomicIncrement(responseCounter);

  GeneralMessageHeader header;
  header.isForwardMessage = false;
  header.type = type;
  header.contentOffset = sizeof(GeneralMessageHeader);
  header.responseData = 0;
  header.responseLen = 0;
  header.responseType = 0;
  header.responseCounter = reinterpret_cast<EncodedPtr>(responseCounter);

  remoteObject()->send(&header, sizeof(GeneralMessageHeader), msg, len, type);

}

void* RegionDataRemote::allocRecv(size_t len, int) {
  return malloc(len);
}

void RegionDataRemote::freeRecv(void* data, size_t , int type) {
  if (type == MessageTypes::CREATEREMOTEREGIONDATAREPLY) {
    free(data);
    return;
  }

  BaseMessageHeader* base = (BaseMessageHeader*)data;
  if (base->isForwardMessage) {
    free(data);
  }

  // Otherwise, the caller of fetchData has to free it.
}

void RegionDataRemote::onRecv(const void* data, size_t len, int type) {
  if (type == MessageTypes::CREATEREMOTEREGIONDATAREPLY) {
    JASSERT(len == sizeof(RemoteRegionHandler));
    memcpy(&_remoteRegionHandler, data, len);
    _isRemoteRegionHandlerReady = true;
    return;
  }

  const BaseMessageHeader* base = (const BaseMessageHeader*)data;
  if (base->isForwardMessage) {
    const ForwardMessageHeader* header = (const ForwardMessageHeader*)data;
    IRegionReplyProxy* proxy = reinterpret_cast<IRegionReplyProxy*>(header->callback);
    proxy->processReplyMsg(base, len, type);

  } else {
    const GeneralMessageHeader* header = (const GeneralMessageHeader*)data;

    const void** responseData = reinterpret_cast<const void**>(header->responseData);
    size_t* responseLen = reinterpret_cast<size_t*>(header->responseLen);
    int* responseType = reinterpret_cast<int*>(header->responseType);
    jalib::AtomicT* responseCounter = reinterpret_cast<jalib::AtomicT*>(header->responseCounter);

    if (responseCounter) {
      jalib::atomicDecrement(responseCounter);

    } else {
      *responseData = data;
      *responseLen = len;
      *responseType = type;
    }
  }
}

// Process remote messages
void RegionDataRemote::forwardMessage(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  size_t len = sizeof(ForwardMessageHeader) + baseLen;

  ForwardMessageHeader* data = (ForwardMessageHeader*)malloc(len);
  data->isForwardMessage = true;
  data->type = base->type;
  data->contentOffset = sizeof(ForwardMessageHeader) + base->contentOffset;
  data->callback = reinterpret_cast<EncodedPtr>(caller);

  memcpy(data->next(), base, baseLen);

  remoteObject()->send(data, len, base->type);
  free(data);
}

void RegionDataRemote::processReadCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processWriteCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processAllocDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processRandomizeDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processUpdateHandlerChainMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller, EncodedPtr) {
  UpdateHandlerChainMessage* msg = (UpdateHandlerChainMessage*)base->content();
  msg->numHops++;
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processGetHostListMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processGetMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processCopyToMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processCopyFromMatrixStorageMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

void RegionDataRemote::processCopyRegionDataSplitMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller) {
  this->forwardMessage(base, baseLen, caller);
}

RegionDataSplitPtr RegionDataRemote::copyRegionDataSplit() {
  JDEBUGASSERT(isDataSplit());

  if (!_localRegionDataSplit) {
    JLOCKSCOPE(_localRegionDataSplitMux);
    if (!_localRegionDataSplit) {
      CopyRegionDataSplitMessage msg;
      void* data;
      size_t len;
      int type;
      this->fetchData(&msg, MessageTypes::COPYREGIONDATASPLIT, sizeof(CopyRegionDataSplitMessage), &data, &len, &type);
      BaseMessageHeader* base = (BaseMessageHeader*)data;
      len = len - base->contentOffset;

      CopyRegionDataSplitReplyMessage* reply = (CopyRegionDataSplitReplyMessage*) base->content();
      _localRegionDataSplit = new RegionDataSplit(_D, _size, reply->splitSize);
      for (int i = 0; i < reply->numParts; ++i) {
        _localRegionDataSplit->setPart(i, reply->handlers()[i]);
      }
      free(data);
      return _localRegionDataSplit;
    }
  }
  return NULL;
}
