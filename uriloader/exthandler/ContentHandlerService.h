#ifndef ContentHandlerService_h
#define ContentHandlerService_h

#include "nsIHandlerService.h"
#include "nsClassHashtable.h"
#include "HandlerServiceChild.h"
#include "nsIMIMEInfo.h"

#define  NS_CONTENTHANDLERSERVICE_CID                                   \
  {0xc4b6fb7c, 0xbfb1, 0x49dc, {0xa6, 0x5f, 0x03, 0x57, 0x96, 0x52, 0x4b, 0x53}}

namespace mozilla {
namespace dom {

class PHandlerServiceChild;

class ContentHandlerService : public nsIHandlerService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERSERVICE

  ContentHandlerService();
  nsresult Init();
  static void nsIHandlerInfoToHandlerInfo(nsIHandlerInfo* aInfo, HandlerInfo* aHandlerInfo);

private:
  virtual ~ContentHandlerService();
  RefPtr<HandlerServiceChild> mHandlerServiceChild;
  nsClassHashtable<nsCStringHashKey, nsCString> mExtToTypeMap;
};

class RemoteHandlerApp : public nsIHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  explicit RemoteHandlerApp(HandlerApp aAppChild) : mAppChild(aAppChild)
  {
  }
private:
  virtual ~RemoteHandlerApp()
  {
  }
  HandlerApp mAppChild;
};


}
}
#endif
