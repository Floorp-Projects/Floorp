/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FinalizationWitnessService.h"

#include "nsString.h"
#include "jsapi.h"
#include "js/CallNonGenericMethod.h"
#include "mozJSComponentLoader.h"
#include "nsZipArchive.h"

#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"


// Implementation of nsIFinalizationWitnessService

static bool gShuttingDown = false;

namespace mozilla {

namespace {

/**
 * An event meant to be dispatched to the main thread upon finalization
 * of a FinalizationWitness, unless method |forget()| has been called.
 *
 * Held as private data by each instance of FinalizationWitness.
 * Important note: we maintain the invariant that these private data
 * slots are already addrefed.
 */
class FinalizationEvent final: public Runnable
{
public:
  FinalizationEvent(const char* aTopic,
                  const char16_t* aValue)
    : mTopic(aTopic)
    , mValue(aValue)
  { }

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService) {
      // This is either too early or, more likely, too late for notifications.
      // Bail out.
      return NS_ERROR_NOT_AVAILABLE;
    }
    (void)observerService->
      NotifyObservers(nullptr, mTopic.get(), mValue.get());
    return NS_OK;
  }
private:
  /**
   * The topic on which to broadcast the notification of finalization.
   *
   * Deallocated on the main thread.
   */
  const nsCString mTopic;

  /**
   * The result of converting the exception to a string.
   *
   * Deallocated on the main thread.
   */
  const nsString mValue;
};

enum {
  WITNESS_SLOT_EVENT,
  WITNESS_INSTANCES_SLOTS
};

/**
 * Extract the FinalizationEvent from an instance of FinalizationWitness
 * and clear the slot containing the FinalizationEvent.
 */
already_AddRefed<FinalizationEvent>
ExtractFinalizationEvent(JSObject *objSelf)
{
  JS::Value slotEvent = JS_GetReservedSlot(objSelf, WITNESS_SLOT_EVENT);
  if (slotEvent.isUndefined()) {
    // Forget() has been called
    return nullptr;
  }

  JS_SetReservedSlot(objSelf, WITNESS_SLOT_EVENT, JS::UndefinedValue());

  return dont_AddRef(static_cast<FinalizationEvent*>(slotEvent.toPrivate()));
}

/**
 * Finalizer for instances of FinalizationWitness.
 *
 * Unless method Forget() has been called, the finalizer displays an error
 * message.
 */
void Finalize(JSFreeOp *fop, JSObject *objSelf)
{
  RefPtr<FinalizationEvent> event = ExtractFinalizationEvent(objSelf);
  if (event == nullptr || gShuttingDown) {
    // NB: event will be null if Forget() has been called
    return;
  }

  // Notify observers. Since we are executed during garbage-collection,
  // we need to dispatch the notification to the main thread.
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  if (mainThread) {
    mainThread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  }
  // We may fail at dispatching to the main thread if we arrive too late
  // during shutdown. In that case, there is not much we can do.
}

static const JSClassOps sWitnessClassOps = {
  nullptr /* addProperty */,
  nullptr /* delProperty */,
  nullptr /* getProperty */,
  nullptr /* setProperty */,
  nullptr /* enumerate */,
  nullptr /* resolve */,
  nullptr /* mayResolve */,
  Finalize /* finalize */
};

static const JSClass sWitnessClass = {
  "FinalizationWitness",
  JSCLASS_HAS_RESERVED_SLOTS(WITNESS_INSTANCES_SLOTS) |
  JSCLASS_FOREGROUND_FINALIZE,
  &sWitnessClassOps
};

bool IsWitness(JS::Handle<JS::Value> v)
{
  return v.isObject() && JS_GetClass(&v.toObject()) == &sWitnessClass;
}


/**
 * JS method |forget()|
 *
 * === JS documentation
 *
 *  Neutralize the witness. Once this method is called, the witness will
 *  never report any error.
 */
bool ForgetImpl(JSContext* cx, const JS::CallArgs& args)
{
  if (args.length() != 0) {
    JS_ReportErrorASCII(cx, "forget() takes no arguments");
    return false;
  }
  JS::Rooted<JS::Value> valSelf(cx, args.thisv());
  JS::Rooted<JSObject*> objSelf(cx, &valSelf.toObject());

  RefPtr<FinalizationEvent> event = ExtractFinalizationEvent(objSelf);
  if (event == nullptr) {
    JS_ReportErrorASCII(cx, "forget() called twice");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool Forget(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<IsWitness, ForgetImpl>(cx, args);
}

static const JSFunctionSpec sWitnessClassFunctions[] = {
  JS_FN("forget", Forget, 0, JSPROP_READONLY | JSPROP_PERMANENT),
  JS_FS_END
};

} // namespace

NS_IMPL_ISUPPORTS(FinalizationWitnessService, nsIFinalizationWitnessService, nsIObserver)

/**
 * Create a new Finalization Witness.
 *
 * A finalization witness is an object whose sole role is to notify
 * observers when it is gc-ed. Once the witness is created, call its
 * method |forget()| to prevent the observers from being notified.
 *
 * @param aTopic The notification topic.
 * @param aValue The notification value. Converted to a string.
 *
 * @constructor
 */
NS_IMETHODIMP
FinalizationWitnessService::Make(const char* aTopic,
                                 const char16_t* aValue,
                                 JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aRetval)
{
  JS::Rooted<JSObject*> objResult(aCx, JS_NewObject(aCx, &sWitnessClass));
  if (!objResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!JS_DefineFunctions(aCx, objResult, sWitnessClassFunctions)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<FinalizationEvent> event = new FinalizationEvent(aTopic, aValue);

  // Transfer ownership of the addrefed |event| to |objResult|.
  JS_SetReservedSlot(objResult, WITNESS_SLOT_EVENT,
                     JS::PrivateValue(event.forget().take()));

  aRetval.setObject(*objResult);
  return NS_OK;
}

NS_IMETHODIMP
FinalizationWitnessService::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const char16_t* aValue)
{
  MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  gShuttingDown = true;
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  return NS_OK;
}

nsresult
FinalizationWitnessService::Init()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  return obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

} // namespace mozilla
