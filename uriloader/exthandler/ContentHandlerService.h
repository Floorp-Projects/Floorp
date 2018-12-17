#ifndef ContentHandlerService_h
#define ContentHandlerService_h

#include "mozilla/dom/PHandlerService.h"
#include "nsIHandlerService.h"
#include "nsClassHashtable.h"
#include "nsIMIMEInfo.h"

namespace mozilla {

class HandlerServiceChild;

namespace dom {

class PHandlerServiceChild;

class ContentHandlerService : public nsIHandlerService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERSERVICE

  ContentHandlerService();
  MOZ_MUST_USE nsresult Init();
  static void nsIHandlerInfoToHandlerInfo(nsIHandlerInfo* aInfo,
                                          HandlerInfo* aHandlerInfo);

 private:
  virtual ~ContentHandlerService();
  RefPtr<HandlerServiceChild> mHandlerServiceChild;
  nsClassHashtable<nsCStringHashKey, nsCString> mExtToTypeMap;
};

class RemoteHandlerApp : public nsIHandlerApp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  explicit RemoteHandlerApp(HandlerApp aAppChild) : mAppChild(aAppChild) {}

 private:
  virtual ~RemoteHandlerApp() {}
  HandlerApp mAppChild;
};

}  // namespace dom
}  // namespace mozilla
#endif
