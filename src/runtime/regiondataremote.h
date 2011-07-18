#ifndef PETABRICKSREGIONDATAREMOTE_H
#define PETABRICKSREGIONDATAREMOTE_H

#include <map>
#include <pthread.h>
#include "regiondatai.h"
#include "remoteobject.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

namespace petabricks {
  using namespace petabricks::RegionDataRemoteMessage;


  class RegionDataRemote;
  typedef jalib::JRef<RegionDataRemote> RegionDataRemotePtr;

  class RegionDataRemoteObject;
  typedef jalib::JRef<RegionDataRemoteObject> RegionDataRemoteObjectPtr;

  class RegionDataRemote : public RegionDataI {
  private:
    RegionDataRemoteObjectPtr _remoteObject;

  public:
    RegionDataRemote(int dimensions, IndexT* size, RegionDataRemoteObjectPtr remoteObject);
    RegionDataRemote(int dimensions, IndexT* size, IndexT* partOffset, RemoteHostPtr host, EncodedPtr remoteRegionData);
    ~RegionDataRemote();

    int allocData();

    ElementT readCell(const IndexT* coord);
    void writeCell(const IndexT* coord, ElementT value);
    DataHostList hosts(IndexT* begin, IndexT* end);

    // Update long chain of RegionHandlers
    UpdateHandlerChainReplyMessage updateHandlerChain();
    UpdateHandlerChainReplyMessage updateHandlerChain(UpdateHandlerChainMessage& msg);

    void onRecv(const void* data, size_t len);
    void fetchData(const void* msg, MessageType type, size_t len, void** responseData, size_t* responseLen);

    // Process remote messages
    void forwardMessage(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);

    void processReadCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processWriteCellMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processGetHostListMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processAllocDataMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);
    void processUpdateHandlerChainMsg(const BaseMessageHeader* base, size_t baseLen, IRegionReplyProxy* caller);

    static RemoteObjectPtr genRemote();
  };


  class RegionDataRemoteObject : public RemoteObject {
  protected:
    RegionDataRemote* _regionData;
  public:
    RegionDataRemoteObject() {}

    void onRecv(const void* data, size_t len) {
      _regionData->onRecv(data, len);
    }

    void onRecvInitial(const void* buf, size_t len);

    RegionDataIPtr regionData() {
      return _regionData;
    }
  };
}

#endif
