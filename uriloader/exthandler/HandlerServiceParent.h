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

  /*
   * Limit the length of MIME types, filename extensions, and protocol
   * schemes we'll consider.
   */
  static const size_t MAX_MIMETYPE_LENGTH = 129; /* Per RFC 6838, type and
                                                    subtype should be limited
                                                    to 64 characters. We add
                                                    one more to account for
                                                    a '/' separator. */
  static const size_t MAX_EXT_LENGTH = 64;       /* Arbitratily chosen to be
                                                    longer than any known
                                                    extension */
  static const size_t MAX_SCHEME_LENGTH = 1024;  /* Arbitratily chosen to be
                                                    longer than any known
                                                    protocol scheme */
};

#endif
