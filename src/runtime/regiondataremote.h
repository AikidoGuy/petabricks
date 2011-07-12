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
  namespace RegionDataRemoteMessage {
    typedef uint16_t MessageType;

    struct MessageTypes {
      enum {
	READCELL = 11,
	WRITECELL,
	GETHOSTLIST,
	UPDATEHANDLERCHAIN,
        ALLOCDATA,
      };
    };

    struct InitialMessageToRegionDataRemote {
      int dimensions;
      IndexT size[MAX_DIMENSIONS];
    };

    struct InitialMessageToRegionMatrixProxy {
      int dimensions;
      IndexT size[MAX_DIMENSIONS];
      IndexT partOffset[MAX_DIMENSIONS];
    };

    struct MessageHeader {
      MessageType type;
      EncodedPtr responseData;
      EncodedPtr responseLen;
    };

    struct ReadCellMessage {
      struct MessageHeader header;
      IndexT coord[MAX_DIMENSIONS];
    };

    struct WriteCellMessage {
      struct MessageHeader header;
      ElementT value;
      IndexT coord[MAX_DIMENSIONS];
    };

    struct GetHostListMessage {
      struct MessageHeader header;
      IndexT begin[MAX_DIMENSIONS];
      IndexT end[MAX_DIMENSIONS];
    };

    struct UpdateHandlerChainMessage {
      struct MessageHeader header;
      HostPid requester;
      int numHops;
    };

    struct AllocDataMessage {
      struct MessageHeader header;
    };

    struct MessageReplyHeader {
      EncodedPtr responseData;
      EncodedPtr responseLen;

      MessageReplyHeader operator=(const MessageHeader& val) {
        responseData = val.responseData;
        responseLen = val.responseLen;
        return *this;
      }
    };

    struct ReadCellReplyMessage {
      struct MessageReplyHeader header;
      ElementT value;
    };

    struct WriteCellReplyMessage {
      struct MessageReplyHeader header;
      ElementT value;
    };

    struct GetHostListReplyMessage {
      struct MessageReplyHeader header;
      int numHosts;
      DataHostListItem hosts[];
    };

    struct UpdateHandlerChainReplyMessage {
      struct MessageReplyHeader header;
      HostPid dataHost;
      int numHops;
      EncodedPtr encodedPtr; // regiondata or remoteobject
    };

    struct AllocDataReplyMessage {
      struct MessageReplyHeader header;
      int result;
    };
  }

  class RegionDataRemote;
  typedef jalib::JRef<RegionDataRemote> RegionDataRemotePtr;

  class RegionDataRemoteObject;
  typedef jalib::JRef<RegionDataRemoteObject> RegionDataRemoteObjectPtr;

  class RegionDataRemote : public RegionDataI {
  private:
    RegionDataRemoteObjectPtr _remoteObject;

  public:
    RegionDataRemote(int dimensions, IndexT* size, RegionDataRemoteObjectPtr remoteObject);
    RegionDataRemote(int dimensions, IndexT* size, IndexT* partOffset, RemoteHostPtr host);
    ~RegionDataRemote() {
      JTRACE("Destruct RegionDataRemote");
    }

    int allocData();

    ElementT readCell(const IndexT* coord);
    void writeCell(const IndexT* coord, ElementT value);
    DataHostList hosts(IndexT* begin, IndexT* end);

    // Update long chain of RegionHandlers
    RegionDataRemoteMessage::UpdateHandlerChainReplyMessage updateHandlerChain();
    RegionDataRemoteMessage::UpdateHandlerChainReplyMessage
      updateHandlerChain(RegionDataRemoteMessage::UpdateHandlerChainMessage& msg);

    void onRecv(const void* data, size_t len);
    void fetchData(const void* msg, size_t len, void** responseData, size_t* responseLen);

    static RemoteObjectPtr genRemote();
  };


  class RegionDataRemoteObject : public RemoteObject {
  protected:
    RegionDataRemote* _regionData;
  public:
    RegionDataRemoteObject() {}
    ~RegionDataRemoteObject() {
      JTRACE("Destruct RegionDataRemoteObject");
    }

    void onRecv(const void* data, size_t len) {
      _regionData->onRecv(data, len);
    }

    void freeRecv(void*, size_t ) { /* do not free */ };

    void onRecvInitial(const void* buf, size_t len);

    RegionDataIPtr regionData() {
      return _regionData;
    }
  };
}

#endif
