#ifndef handler_service_child_h
#define handler_service_child_h

#include "mozilla/dom/PHandlerServiceChild.h"

namespace mozilla {

class HandlerServiceChild final : public mozilla::dom::PHandlerServiceChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(HandlerServiceChild)
  HandlerServiceChild() {}

 private:
  virtual ~HandlerServiceChild() {}
};

}  // namespace mozilla

#endif
