#include "regionhandler.h"

#include "regiondataraw.h"
#include "regiondataremote.h"
#include "regiondatasplit.h"
#include "regionmatrixproxy.h"

using namespace petabricks;

RegionHandler::RegionHandler(const int dimensions, const IndexT* size) {
  _regionData = new RegionDataRaw(dimensions, size);
}

RegionHandler::RegionHandler(const RegionDataIPtr regionData) {
  _regionData = regionData;
}

RegionHandler::RegionHandler(const EncodedPtr remoteObjPtr) {
  RegionDataRemoteObject* remoteObj = reinterpret_cast<RegionDataRemoteObject*>(remoteObjPtr);
  _regionData = remoteObj->regionData();
}

ElementT RegionHandler::readCell(const IndexT* coord) {
  return _regionData->readCell(coord);
}

void RegionHandler::writeCell(const IndexT* coord, ElementT value) {
  _regionData->writeCell(coord, value);
}

int RegionHandler::allocData() {
  return _regionData->allocData();
}

RegionDataIPtr RegionHandler::getRegionData() {
  return _regionData;
}

void RegionHandler::updateRegionData(RegionDataIPtr regionData) {
  _regionData = regionData;
}

DataHostList RegionHandler::hosts(IndexT* begin, IndexT* end) {
  return _regionData->hosts(begin, end);
}

int RegionHandler::dimensions() {
  return _regionData->dimensions();
}

IndexT* RegionHandler::size() {
  return _regionData->size();
}

RegionDataType RegionHandler::type() const {
  return _regionData->type();
}

//
// Migration
//

EncodedPtr RegionHandler::moveToRemoteHost(RemoteHostPtr host) {
  RegionMatrixProxyPtr proxy = new RegionMatrixProxy(this);
  RegionMatrixProxyRemoteObjectPtr local = proxy->genLocal();

  // InitialMsg
  RegionDataRemoteMessage::InitialMessage msg = RegionDataRemoteMessage::InitialMessage();
  msg.dimensions = dimensions();
  memcpy(msg.size, size(), sizeof(msg.size));
  int len = sizeof(RegionDataRemoteMessage::InitialMessage);

  host->createRemoteObject(local.asPtr(), &RegionDataRemote::genRemote, &msg, len);
  local->waitUntilCreated();
  return local->remoteObjPtr();
}

void RegionHandler::updateHandlerChain() {
  if (type() == RegionDataTypes::REGIONDATAREMOTE) {
    RegionDataRemoteMessage::UpdateHandlerChainReplyMessage* reply =
      ((RegionDataRemote*)_regionData.asPtr())->updateHandlerChain();
    JTRACE("updatehandler")(reply->dataHost)(reply->numHops)(reply->regionData.asPtr());

    if (reply->dataHost == HostPid::self()) {
      // Data is in the same process. Update handler to point directly to the data.
      updateRegionData(reply->regionData);
    } else if (reply->numHops > 1) {
      // Multiple network hops to data. Create a direct connection to data.

      // (yod) TODO:
      //this->updateHandler(999);
    }
  }
}

//
// RegionDataSplit
//

void RegionHandler::splitData(IndexT* splitSize) {
  JASSERT(type() == RegionDataTypes::REGIONDATARAW);
  RegionDataIPtr newRegionData =
    new RegionDataSplit((RegionDataRaw*)_regionData.asPtr(), splitSize);
  updateRegionData(newRegionData);
}

void RegionHandler::createDataPart(int partIndex, RemoteHostPtr host) {
  JASSERT(type() == RegionDataTypes::REGIONDATASPLIT);
  ((RegionDataSplit*)_regionData.asPtr())->createPart(partIndex, host);
}

