#include "HangDetails.h"
#include "nsIHangDetails.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/GfxMessageUtils.h" // For ParamTraits<GeckoProcessType>
#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"
#endif

namespace mozilla {

NS_IMETHODIMP
nsHangDetails::GetDuration(uint32_t* aDuration)
{
  *aDuration = mDetails.mDuration;
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetThread(nsACString& aName)
{
  aName.Assign(mDetails.mThreadName);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetRunnableName(nsACString& aRunnableName)
{
  aRunnableName.Assign(mDetails.mRunnableName);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetProcess(nsACString& aName)
{
  aName.AssignASCII(XRE_ChildProcessTypeToString(mDetails.mProcess));
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetRemoteType(nsAString& aName)
{
  aName.Assign(mDetails.mRemoteType);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetAnnotations(JSContext* aCx, JS::MutableHandleValue aVal)
{
  // We create an object with { "key" : "value" } string pairs for each item in
  // our annotations object.
  JS::RootedObject jsAnnotation(aCx, JS_NewPlainObject(aCx));
  if (!jsAnnotation) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (auto& annot : mDetails.mAnnotations) {
    JSString* jsString = JS_NewUCStringCopyN(aCx, annot.mValue.get(), annot.mValue.Length());
    if (!jsString) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    JS::RootedValue jsValue(aCx);
    jsValue.setString(jsString);
    if (!JS_DefineUCProperty(aCx, jsAnnotation, annot.mName.get(), annot.mName.Length(),
                             jsValue, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aVal.setObject(*jsAnnotation);
  return NS_OK;
}

namespace  {

nsresult
StringFrame(JSContext* aCx,
            JS::RootedObject& aTarget,
            size_t aIndex,
            const char* aString)
{
  JSString* jsString = JS_NewStringCopyZ(aCx, aString);
  if (!jsString) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  JS::RootedString string(aCx, jsString);
  if (!string) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!JS_DefineElement(aCx, aTarget, aIndex, string, JSPROP_ENUMERATE)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // anonymous namespace

NS_IMETHODIMP
nsHangDetails::GetStack(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  JS::RootedObject ret(aCx, JS_NewArrayObject(aCx, mDetails.mStack.length()));
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < mDetails.mStack.length(); ++i) {
    const HangStack::Frame& frame = mDetails.mStack[i];
    switch (frame.GetKind()) {
      case HangStack::Frame::Kind::STRING: {
        nsresult rv = StringFrame(aCx, ret, i, frame.AsString());
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      case HangStack::Frame::Kind::MODOFFSET: {
        JS::RootedObject jsFrame(aCx, JS_NewArrayObject(aCx, 2));
        if (!jsFrame) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, jsFrame, 0, frame.AsModOffset().mModule, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsPrintfCString hexString("%" PRIxPTR, (uintptr_t)frame.AsModOffset().mOffset);
        JS::RootedString hex(aCx, JS_NewStringCopyZ(aCx, hexString.get()));
        if (!hex || !JS_DefineElement(aCx, jsFrame, 1, hex, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, ret, i, jsFrame, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        break;
      }

      case HangStack::Frame::Kind::PC: {
        JS::RootedObject jsFrame(aCx, JS_NewArrayObject(aCx, 2));
        if (!jsFrame) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, jsFrame, 0, -1, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsPrintfCString hexString("%" PRIxPTR, frame.AsPC());
        JS::RootedString hex(aCx, JS_NewStringCopyZ(aCx, hexString.get()));
        if (!hex || !JS_DefineElement(aCx, jsFrame, 1, hex, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, ret, i, jsFrame, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        break;
      }

      case HangStack::Frame::Kind::CONTENT: {
        nsresult rv = StringFrame(aCx, ret, i, "(content script)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      case HangStack::Frame::Kind::JIT: {
        nsresult rv = StringFrame(aCx, ret, i, "(jit frame)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      case HangStack::Frame::Kind::WASM: {
        nsresult rv = StringFrame(aCx, ret, i, "(wasm)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      case HangStack::Frame::Kind::SUPPRESSED: {
        nsresult rv = StringFrame(aCx, ret, i, "(profiling suppressed)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE("Invalid variant");
        break;
    }
  }

  aVal.setObject(*ret);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetModules(JSContext* aCx, JS::MutableHandleValue aVal)
{
  auto& modules = mDetails.mStack.GetModules();
  size_t length = modules.Length();
  JS::RootedObject retObj(aCx, JS_NewArrayObject(aCx, length));
  if (!retObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < length; ++i) {
    const HangStack::Module& module = modules[i];
    JS::RootedObject jsModule(aCx, JS_NewArrayObject(aCx, 2));
    if (!jsModule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString name(aCx, JS_NewUCStringCopyN(aCx, module.mName.BeginReading(), module.mName.Length()));
    if (!JS_DefineElement(aCx, jsModule, 0, name, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString breakpadId(aCx, JS_NewStringCopyN(aCx, module.mBreakpadId.BeginReading(), module.mBreakpadId.Length()));
    if (!JS_DefineElement(aCx, jsModule, 1, breakpadId, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_DefineElement(aCx, retObj, i, jsModule, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aVal.setObject(*retObj);
  return NS_OK;
}

// Processing and submitting the stack as an observer notification.

void
nsHangDetails::Submit()
{
  if (NS_WARN_IF(!SystemGroup::Initialized())) {
    return;
  }

  RefPtr<nsHangDetails> hangDetails = this;
  nsCOMPtr<nsIRunnable> notifyObservers = NS_NewRunnableFunction("NotifyBHRHangObservers", [hangDetails] {
    // The place we need to report the hang to varies depending on process.
    //
    // In child processes, we report the hang to our parent process, while if
    // we're in the parent process, we report a bhr-thread-hang observer
    // notification.
    switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content: {
      auto cc = dom::ContentChild::GetSingleton();
      if (cc) {
        hangDetails->mDetails.mRemoteType.Assign(cc->GetRemoteType());
        Unused << cc->SendBHRThreadHang(hangDetails->mDetails);
      }
      break;
    }
    case GeckoProcessType_GPU: {
      auto gp = gfx::GPUParent::GetSingleton();
      if (gp) {
        Unused << gp->SendBHRThreadHang(hangDetails->mDetails);
      }
      break;
    }
    case GeckoProcessType_Default: {
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        os->NotifyObservers(hangDetails, "bhr-thread-hang", nullptr);
      }
      break;
    }
    default:
      // XXX: Consider handling GeckoProcessType_GMPlugin and
      // GeckoProcessType_Plugin?
      NS_WARNING("Unsupported BHR process type - discarding hang.");
      break;
    }
#ifdef MOZ_GECKO_PROFILER
    if (profiler_is_active()) {
      TimeStamp endTime = hangDetails->mDetails.mEndTime;
      TimeStamp startTime = endTime -
                            TimeDuration::FromMilliseconds(hangDetails->mDetails.mDuration);
      profiler_add_marker(
        "BHR-detected hang",
        MakeUnique<HangMarkerPayload>(startTime, endTime));
    }
#endif
  });

  nsresult rv = SystemGroup::Dispatch(TaskCategory::Other,
                                      notifyObservers.forget());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMPL_ISUPPORTS(nsHangDetails, nsIHangDetails)

NS_IMETHODIMP
ProcessHangStackRunnable::Run()
{
  // NOTE: Reading module information can take a long time, which is why we do
  // it off-main-thread.
  mHangDetails.mStack.ReadModuleInformation();

  RefPtr<nsHangDetails> hangDetails = new nsHangDetails(Move(mHangDetails));
  hangDetails->Submit();

  return NS_OK;
}

} // namespace mozilla


/**
 * IPC Serialization / Deserialization logic
 */
namespace IPC {

void
ParamTraits<mozilla::HangDetails>::Write(Message* aMsg, const mozilla::HangDetails& aParam)
{
  WriteParam(aMsg, aParam.mDuration);
  WriteParam(aMsg, aParam.mProcess);
  WriteParam(aMsg, aParam.mRemoteType);
  WriteParam(aMsg, aParam.mThreadName);
  WriteParam(aMsg, aParam.mRunnableName);
  WriteParam(aMsg, aParam.mStack);
  WriteParam(aMsg, aParam.mAnnotations);
}

bool
ParamTraits<mozilla::HangDetails>::Read(const Message* aMsg,
                                        PickleIterator* aIter,
                                        mozilla::HangDetails* aResult)
{
  if (!ReadParam(aMsg, aIter, &aResult->mDuration)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mProcess)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mRemoteType)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mThreadName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mRunnableName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mStack)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mAnnotations)) {
    return false;
  }

  return true;
}

} // namespace IPC
