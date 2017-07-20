#include "HangDetails.h"
#include "nsIHangDetails.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/GfxMessageUtils.h" // For ParamTraits<GeckoProcessType>

namespace mozilla {

HangDetails::HangDetails(const HangDetails& aOther)
  : mDuration(aOther.mDuration)
  , mThreadName(aOther.mThreadName)
  , mRunnableName(aOther.mRunnableName)
  , mPseudoStack(aOther.mPseudoStack)
  , mAnnotations(nullptr)
  , mStack(aOther.mStack)
{
  // XXX: The current implementation of mAnnotations is really suboptimal here.
  if (!aOther.mAnnotations) {
    return;
  }

  mAnnotations = mozilla::HangMonitor::CreateEmptyHangAnnotations();
  auto enumerator = aOther.mAnnotations->GetEnumerator();
  if (enumerator) {
    nsAutoString key;
    nsAutoString value;
    while (enumerator->Next(key, value)) {
      mAnnotations->AddAnnotation(key, value);
    }
  }
}

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
nsHangDetails::GetStack(JSContext* aCx, JS::MutableHandleValue aVal)
{
  size_t length = mDetails.mStack.GetStackSize();
  JS::RootedObject retObj(aCx, JS_NewArrayObject(aCx, length));
  if (!retObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < length; ++i) {
    auto& frame = mDetails.mStack.GetFrame(i);
    JS::RootedObject jsFrame(aCx, JS_NewArrayObject(aCx, 2));
    if (!jsFrame) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_DefineElement(aCx, jsFrame, 0, frame.mModIndex, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsPrintfCString hexString("%" PRIxPTR, frame.mOffset);
    JS::RootedString hex(aCx, JS_NewStringCopyZ(aCx, hexString.get()));
    if (!hex || !JS_DefineElement(aCx, jsFrame, 1, hex, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_DefineElement(aCx, retObj, i, jsFrame, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aVal.setObject(*retObj);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetModules(JSContext* aCx, JS::MutableHandleValue aVal)
{
  size_t length = mDetails.mStack.GetNumModules();
  JS::RootedObject retObj(aCx, JS_NewArrayObject(aCx, length));
  if (!retObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < length; ++i) {
    auto& module = mDetails.mStack.GetModule(i);
    JS::RootedObject jsModule(aCx, JS_NewArrayObject(aCx, 2));
    if (!jsModule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString name(aCx, JS_NewUCStringCopyZ(aCx, module.mName.get()));
    if (!name || !JS_DefineElement(aCx, jsModule, 0, name, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString id(aCx, JS_NewStringCopyZ(aCx, module.mBreakpadId.c_str()));
    if (!id || !JS_DefineElement(aCx, jsModule, 1, id, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_DefineElement(aCx, retObj, i, jsModule, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aVal.setObject(*retObj);
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

  if (!mDetails.mAnnotations) {
    aVal.setObject(*jsAnnotation);
    return NS_OK;
  }

  // Loop over all of the annotations in the enumerator and add them.
  auto enumerator = mDetails.mAnnotations->GetEnumerator();
  if (enumerator) {
    nsAutoString key;
    nsAutoString value;
    while (enumerator->Next(key, value)) {
      JS::RootedValue jsValue(aCx);
      jsValue.setString(JS_NewUCStringCopyN(aCx, value.get(), value.Length()));
      if (!JS_DefineUCProperty(aCx, jsAnnotation, key.get(), key.Length(),
                               jsValue, JSPROP_ENUMERATE)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  aVal.setObject(*jsAnnotation);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetPseudoStack(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  JS::RootedObject ret(aCx, JS_NewArrayObject(aCx, mDetails.mPseudoStack.length()));
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < mDetails.mPseudoStack.length(); ++i) {
    JS::RootedString string(aCx, JS_NewStringCopyZ(aCx, mDetails.mPseudoStack[i]));
    if (!string) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!JS_DefineElement(aCx, ret, i, string, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aVal.setObject(*ret);
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
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(hangDetails, "bhr-thread-hang", nullptr);
    }
  });

  nsresult rv = SystemGroup::Dispatch(TaskCategory::Other,
                                      notifyObservers.forget());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMPL_ISUPPORTS(nsHangDetails, nsIHangDetails)

NS_IMETHODIMP
ProcessHangStackRunnable::Run()
{
  // NOTE: This is an expensive operation on some platforms, so we do it off of
  // any other critical path, moving it onto the StreamTransportService.
  mHangDetails.mStack = Telemetry::GetStackAndModules(mNativeStack);

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
  WriteParam(aMsg, aParam.mThreadName);
  WriteParam(aMsg, aParam.mRunnableName);
  WriteParam(aMsg, aParam.mPseudoStack);

  // Write out the annotation information
  // If we have no annotations, write out a 0-length annotations.
  // XXX: Everything about HangAnnotations is awful.
  if (!aParam.mAnnotations) {
    WriteParam(aMsg, (size_t) 0);
  } else {
    size_t length = aParam.mAnnotations->Count();
    WriteParam(aMsg, length);
    auto enumerator = aParam.mAnnotations->GetEnumerator();
    if (enumerator) {
      nsAutoString key;
      nsAutoString value;
      while (enumerator->Next(key, value)) {
        WriteParam(aMsg, key);
        WriteParam(aMsg, value);
      }
    }
  }

  // NOTE: ProcessedStack will stop being used for BHR in bug 1367406, so this
  // inline serialization will survive until then.

  // Write out the native stack module information
  {
    size_t length = aParam.mStack.GetNumModules();
    WriteParam(aMsg, length);
    for (size_t i = 0; i < length; ++i) {
      auto& module = aParam.mStack.GetModule(i);
      WriteParam(aMsg, module.mName);
      WriteParam(aMsg, module.mBreakpadId);
    }
  }

  // Native stack frame information
  {
    size_t length = aParam.mStack.GetStackSize();
    WriteParam(aMsg, length);
    for (size_t i = 0; i < length; ++i) {
      auto& frame = aParam.mStack.GetFrame(i);
      WriteParam(aMsg, frame.mOffset);
      WriteParam(aMsg, frame.mModIndex);
    }
  }
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
  if (!ReadParam(aMsg, aIter, &aResult->mThreadName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mRunnableName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mPseudoStack)) {
    return false;
  }

  // Annotation information
  {
    aResult->mAnnotations =
      mozilla::HangMonitor::CreateEmptyHangAnnotations();

    size_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    for (size_t i = 0; i < length; ++i) {
      nsAutoString key;
      if (!ReadParam(aMsg, aIter, &key)) {
        return false;
      }
      nsAutoString value;
      if (!ReadParam(aMsg, aIter, &value)) {
        return false;
      }
      aResult->mAnnotations->AddAnnotation(key, value);
    }
  }

  // NOTE: ProcessedStack will stop being used for BHR in bug 1367406, so this
  // inline serialization will survive until then.

  // Native Stack Module Information
  {
    size_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    for (size_t i = 0; i < length; ++i) {
      mozilla::Telemetry::ProcessedStack::Module module;
      if (!ReadParam(aMsg, aIter, &module.mName)) {
        return false;
      }
      if (!ReadParam(aMsg, aIter, &module.mBreakpadId)) {
        return false;
      }
      aResult->mStack.AddModule(module);
    }
  }

  // Native stack frame information
  {
    size_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    for (size_t i = 0; i < length; ++i) {
      mozilla::Telemetry::ProcessedStack::Frame frame;
      if (!ReadParam(aMsg, aIter, &frame.mOffset)) {
        return false;
      }
      if (!ReadParam(aMsg, aIter, &frame.mModIndex)) {
        return false;
      }
      aResult->mStack.AddFrame(frame);
    }
  }

  return true;
}

} // namespace IPC
