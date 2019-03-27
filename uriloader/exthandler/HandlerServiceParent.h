#ifndef handler_service_parent_h
#define handler_service_parent_h

#include "mozilla/dom/PHandlerServiceParent.h"
#include "nsIMIMEInfo.h"

class nsIHandlerApp;

class HandlerServiceParent final : public mozilla::dom::PHandlerServiceParent {
 public:
  HandlerServiceParent();
  NS_INLINE_DECL_REFCOUNTING(HandlerServiceParent)

 private:
  virtual ~HandlerServiceParent();
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvFillHandlerInfo(
      const HandlerInfo& aHandlerInfoData, const nsCString& aOverrideType,
      HandlerInfo* handlerInfoData) override;

  mozilla::ipc::IPCResult RecvGetMIMEInfoFromOS(const nsCString& aMIMEType,
                                                const nsCString& aExtension,
                                                nsresult* aRv,
                                                HandlerInfo* aHandlerInfoData,
                                                bool* aFound) override;

  mozilla::ipc::IPCResult RecvExists(const HandlerInfo& aHandlerInfo,
                                     bool* exists) override;

  mozilla::ipc::IPCResult RecvGetTypeFromExtension(
      const nsCString& aFileExtension, nsCString* type) override;

  mozilla::ipc::IPCResult RecvExistsForProtocolOS(
      const nsCString& aProtocolScheme, bool* aHandlerExists) override;

  mozilla::ipc::IPCResult RecvExistsForProtocol(
      const nsCString& aProtocolScheme, bool* aHandlerExists) override;

  mozilla::ipc::IPCResult RecvGetApplicationDescription(
      const nsCString& aScheme, nsresult* aRv, nsString* aDescription) override;
};

#endif
