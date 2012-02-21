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

  host->createRemoteObject(this, &RegionMatrixProxy::genRemote, buf, msg_len);
}

RegionDataRemote::RegionDataRemote(const int dimensions, const IndexT* size, const HostPid& hostPid, const EncodedPtr remoteHandler) {
  init(dimensions, size);

  // Defer the initialization of _remoteObject
  _remoteRegionHandler.hostPid = hostPid;
  _remoteRegionHandler.remoteHandler = remoteHandler;
}

void RegionDataRemote::init(const int dimensions, const IndexT* size) {
  _D = dimensions;
  _type = RegionDataTypes::REGIONDATAREMOTE;

  memcpy(_size, size, sizeof(IndexT) * _D);
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
  AllocDataReplyMessage* reply = (AllocDataReplyMessage*)data;

  ElementT result = reply->result;
  free(reply);
  return result;
}

void RegionDataRemote::randomize() {
  void* data;
  size_t len;
  int type;
  this->fetchData(0, MessageTypes::RANDOMIZEDATA, 0, &data, &len, &type);

  free(data);
}

const RemoteRegionHandler* RegionDataRemote::remoteRegionHandler() const {
  if (_remoteRegionHandler.remoteHandler == 0) {
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
  ReadCellReplyMessage* reply = (ReadCellReplyMessage*)data;
  ElementT value = reply->value;
  free(reply);
  return value;
}

void RegionDataRemote::readByCache(void* request, size_t request_len, void* reply, size_t &/*reply_len*/) const {
  void* data;
  size_t len;
  int type;
  this->fetchData(request, MessageTypes::READCELLCACHE, request_len, &data, &len, &type);
  ReadCellCacheReplyMessage* r = (ReadCellCacheReplyMessage*)data;

  RegionDataRemoteCacheLine* cacheLine = (RegionDataRemoteCacheLine*)reply;
  cacheLine->start = r->start;
  cacheLine->end = r->end;
  memcpy(cacheLine->base, r->values, sizeof(ElementT) * (r->end + 1));
  free(r);
}

void RegionDataRemote::writeCell(const IndexT* coord, ElementT value) {
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
  WriteCellReplyMessage* reply = (WriteCellReplyMessage*)data;

  free(reply);
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
  WriteCellReplyMessage* reply = (WriteCellReplyMessage*)data;

  free(reply);
}

RegionDataIPtr RegionDataRemote::copyToScratchMatrixStorage(CopyToMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize) const {
  void* data;
  size_t replyLen;
  int type;
  this->fetchData(origMsg, MessageTypes::TOSCRATCHSTORAGE, len, &data, &replyLen, &type);

  if (type == MessageTypes::COPYREGIONDATASPLIT) {
    // Copy regiondatasplit to local
    CopyRegionDataSplitReplyMessage* reply = (CopyRegionDataSplitReplyMessage*)data;
    RegionDataSplitPtr regionDataSplit = createRegionDataSplit(reply);
    regionDataSplit->copyToScratchMatrixStorage(origMsg, len, scratchStorage, scratchMetadata, scratchStorageSize);
    free(data);
    return regionDataSplit.asPtr();

  } else if (type == MessageTypes::TOSCRATCHSTORAGE) {
    CopyToMatrixStorageReplyMessage* reply = (CopyToMatrixStorageReplyMessage*)data;
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

  } else {
    JASSERT(false).Text("Unknown reply type");
  }

  free(data);
  return NULL;
}

void RegionDataRemote::copyFromScratchMatrixStorage(CopyFromMatrixStorageMessage* origMsg, size_t len, MatrixStoragePtr scratchStorage, RegionMatrixMetadata* scratchMetadata, const IndexT* scratchStorageSize) {
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

  size_t msgLen =  RegionMatrixMetadata::len(origMetadata->dimensions, origMetadata->numSliceDimensions) + (sizeof(ElementT) * storageCount);
  JASSERT(msgLen <= len);

  void* data;
  size_t replyLen;
  int type;
  this->fetchData(origMsg, MessageTypes::FROMSCRATCHSTORAGE, msgLen, &data, &replyLen, &type);
  free(data);
}

RegionDataIPtr RegionDataRemote::hosts(const IndexT* begin, const IndexT* end, DataHostPidList& list) {
  GetHostListMessage msg;
  memcpy(msg.begin, begin, _D * sizeof(IndexT));
  memcpy(msg.end, end, _D * sizeof(IndexT));

  void* data;
  size_t len;
  int type;
  this->fetchData(&msg, MessageTypes::GETHOSTLIST, sizeof(GetHostListMessage), &data, &len, &type);
  GetHostListReplyMessage* reply = (GetHostListReplyMessage*)data;

  for (int i = 0; i < reply->numHosts; i++) {
    list.push_back(reply->hosts[i]);
  }

  size_t getHostListSize =  sizeof(GetHostListReplyMessage) + (reply->numHosts * sizeof(DataHostPidListItem));
  if (getHostListSize < len) {
    // RemoteData is RegionDataSplit. We will create a local copy.
    CopyRegionDataSplitReplyMessage* copyMsg = (CopyRegionDataSplitReplyMessage*)((char*)data + getHostListSize);
    RegionDataSplitPtr regionDataSplit = createRegionDataSplit(copyMsg);
    free(reply);
    return regionDataSplit.asPtr();

  } else {
    free(reply);
    return NULL;
  }
}

RemoteHostPtr RegionDataRemote::dataHost() {
  return remoteObject()->host();
}

UpdateHandlerChainReplyMessage RegionDataRemote::updateHandlerChain(UpdateHandlerChainMessage& msg) {
  msg.numHops += 1;

  void* data;
  size_t len;
  int type;
  this->fetchData(&msg, MessageTypes::UPDATEHANDLERCHAIN, sizeof(UpdateHandlerChainMessage), &data, &len, &type);
  UpdateHandlerChainReplyMessage* _reply = (UpdateHandlerChainReplyMessage*)data;

  UpdateHandlerChainReplyMessage reply = *_reply;
  free(_reply);
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

  size_t dataLen = sizeof(GeneralMessageHeader) + len;

  GeneralMessageHeader* header = (GeneralMessageHeader*)malloc(dataLen);
  header->isForwardMessage = false;
  header->type = type;
  header->contentOffset = sizeof(GeneralMessageHeader);
  header->responseData = reinterpret_cast<EncodedPtr>(responseData);
  header->responseLen = reinterpret_cast<EncodedPtr>(responseLen);
  header->responseType = reinterpret_cast<EncodedPtr>(responseType);

  memcpy(header->content(), msg, len);

  remoteObject()->send(header, dataLen);
  free(header);

  JLOCKSCOPE(*this);
  // wait for the data
  while (*responseData == 0 || *responseLen == 0) {
    this->waitMsgMu();
  }
}

void RegionDataRemote::onRecv(const void* data, size_t len, int type) {
  if (type == MessageTypes::CREATEREMOTEREGIONDATAREPLY) {
    JASSERT(len == sizeof(RemoteRegionHandler));
    memcpy(&_remoteRegionHandler, data, len);
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

    size_t sz = len - base->contentOffset;
    void* msg = malloc(sz);
    memcpy(msg, base->content(), sz);

    *responseData = msg;
    *responseLen = sz;
    *responseType = type;
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

  remoteObject()->send(data, len);
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

RegionDataSplitPtr RegionDataRemote::createRegionDataSplit(CopyRegionDataSplitReplyMessage* msg) const {
    RegionDataSplitPtr regionDataSplit = new RegionDataSplit(_D, _size, msg->splitSize);
    for (int i = 0; i < msg->numParts; ++i) {
      regionDataSplit->setPart(i, msg->handlers()[i]);
    }
    return regionDataSplit;
}
