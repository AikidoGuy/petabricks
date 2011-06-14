#include "regiondataremote.h"

#include "regionmatrix.h"

using namespace petabricks;
using namespace petabricks::RegionDataRemoteMessage;

RegionDataRemote::RegionDataRemote(int dimensions, IndexT* size, RemoteObjectPtr remoteObject) {
  _D = dimensions;
  _type = RegionDataTypes::REGIONDATAREMOTE;
  _size = size;
  _remoteObject = remoteObject;

  pthread_mutex_init(&_seq_mux, NULL);
  pthread_mutex_init(&_buffer_mux, NULL);
  pthread_cond_init(&_buffer_cond, NULL);
  _seq = 0;
  _recv_seq = 0;
}

RegionDataRemote::~RegionDataRemote() {
  pthread_mutex_destroy(&_seq_mux);
  pthread_mutex_destroy(&_buffer_mux);
  pthread_cond_destroy(&_buffer_cond);
}

int RegionDataRemote::allocData() {
  // TODO: this should not be called
  return -1;
}

void* RegionDataRemote::fetchData(const void* msg, size_t len) {
  pthread_mutex_lock(&_seq_mux);
  _remoteObject->send(msg, len);
  uint16_t seq = ++_seq;
  pthread_mutex_unlock(&_seq_mux);

  // wait for the data
  pthread_mutex_lock(&_buffer_mux);
  while (seq > _recv_seq) {
    pthread_cond_wait(&_buffer_cond, &_buffer_mux);
  }

  void* ret = _buffer[seq];
  _buffer.erase(seq);

  pthread_mutex_unlock(&_buffer_mux);

  // wake other threads
  pthread_cond_broadcast(&_buffer_cond);

  return ret;
}

ElementT RegionDataRemote::readCell(const IndexT* coord) {
  ReadCellMessage* msg = new ReadCellMessage();
  msg->type = MessageTypes::READCELL;
  memcpy(msg->coord, coord, _D * sizeof(IndexT));

  ElementT elmt = *(ElementT*)this->fetchData(msg, sizeof *msg);

  delete msg;
  return elmt;
}

void RegionDataRemote::writeCell(const IndexT* coord, ElementT value) {
  WriteCellMessage* msg = new WriteCellMessage();
  msg->type = MessageTypes::WRITECELL;
  msg->value = value;
  memcpy(msg->coord, coord, _D * sizeof(IndexT));

  ElementT elmt = *(ElementT*)this->fetchData(msg, sizeof *msg);

  delete msg;
  JASSERT(elmt == value);
}

DataHostList RegionDataRemote::hosts(IndexT* begin, IndexT* end) {
  GetHostListMessage* msg = new GetHostListMessage();
  msg->type = MessageTypes::GETHOSTLIST;
  memcpy(msg->begin, begin, _D * sizeof(IndexT));
  memcpy(msg->end, end, _D * sizeof(IndexT));

  GetHostListReplyMessage* reply = (GetHostListReplyMessage*)this->fetchData(msg, sizeof *msg);
  delete msg;

  DataHostList list;
  for (int i = 0; i < reply->numHosts; i++) {
    list.push_back(reply->hosts[i]);
  }
  return list;
}

UpdateHandlerChainReplyMessage* RegionDataRemote::updateHandlerChain(UpdateHandlerChainMessage* msg) {
  UpdateHandlerChainReplyMessage* reply =
    (UpdateHandlerChainReplyMessage*)this->fetchData(msg, sizeof *msg);
  reply->numHops += 1;
  return reply;
}

UpdateHandlerChainReplyMessage* RegionDataRemote::updateHandlerChain() {
  UpdateHandlerChainMessage* msg = new UpdateHandlerChainMessage();
  msg->type = MessageTypes::UPDATEHANDLERCHAIN;
  msg->requester = HostPid::self();
  UpdateHandlerChainReplyMessage* reply = this->updateHandlerChain(msg);
  delete msg;
  return reply;
}

void RegionDataRemote::onRecv(const void* data, size_t len) {
  void* x = malloc(len);
  memmove(x, data, len);
  pthread_mutex_lock(&_buffer_mux);
  _buffer[++_recv_seq] = x;
  pthread_mutex_unlock(&_buffer_mux);
  pthread_cond_broadcast(&_buffer_cond);
}

RemoteObjectPtr RegionDataRemote::genRemote() {
  return new RegionDataRemoteObject();
}

//
// RegionDataRemoteObject
//

void RegionDataRemoteObject::onRecvInitial(const void* buf, size_t len) {
  JASSERT(len == sizeof(InitialMessage));
  InitialMessage* msg = (InitialMessage*) buf;

  IndexT* size = (IndexT*)malloc(sizeof(IndexT) * msg->dimensions);
  memcpy(size, msg->size, sizeof(IndexT) * msg->dimensions);

  _regionData = new RegionDataRemote(msg->dimensions, size, this);

  RegionMatrix::addMovingBuffer((RegionDataIPtr) _regionData.asPtr(), msg->movingBufferIndex);
}

