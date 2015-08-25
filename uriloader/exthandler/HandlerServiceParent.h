#ifndef handler_service_parent_h
#define handler_service_parent_h

#include "mozilla/dom/PHandlerServiceParent.h"
#include "nsIMIMEInfo.h"

class nsIHandlerApp;

class HandlerServiceParent final : public mozilla::dom::PHandlerServiceParent
{
 public:
  HandlerServiceParent();
  NS_INLINE_DECL_REFCOUNTING(HandlerServiceParent)

 private:
  virtual ~HandlerServiceParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;


  virtual bool RecvFillHandlerInfo(const HandlerInfo& aHandlerInfoData,
                                   const nsCString& aOverrideType,
                                   HandlerInfo* handlerInfoData) override;
  virtual bool RecvExists(const HandlerInfo& aHandlerInfo,
                          bool* exits) override;

  virtual bool RecvGetTypeFromExtension(const nsCString& aFileExtension,
                                        nsCString* type) override;

};

#endif
