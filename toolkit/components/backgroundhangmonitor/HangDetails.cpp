#include "HangDetails.h"
#include "nsIHangDetails.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/GfxMessageUtils.h" // For ParamTraits<GeckoProcessType>

#ifdef MOZ_GECKO_PROFILER
#include "shared-libraries.h"
#endif

namespace mozilla {

NS_IMETHODIMP
nsHangDetails::GetDuration(double* aDuration)
{
  *aDuration = mDetails.duration().ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetThread(nsACString& aName)
{
  aName.Assign(mDetails.threadName());
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetRunnableName(nsACString& aRunnableName)
{
  aRunnableName.Assign(mDetails.runnableName());
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetProcess(nsACString& aName)
{
  aName.Assign(mDetails.process());
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetRemoteType(nsAString& aName)
{
  aName.Assign(mDetails.remoteType());
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

  for (auto& annot : mDetails.annotations()) {
    JSString* jsString = JS_NewUCStringCopyN(aCx, annot.value().get(), annot.value().Length());
    if (!jsString) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    JS::RootedValue jsValue(aCx);
    jsValue.setString(jsString);
    if (!JS_DefineUCProperty(aCx, jsAnnotation, annot.name().get(), annot.name().Length(),
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
nsHangDetails::GetStack(JSContext* aCx, JS::MutableHandleValue aStack)
{
  auto& stack = mDetails.stack();
  uint32_t length = stack.stack().Length();
  JS::RootedObject ret(aCx, JS_NewArrayObject(aCx, length));
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < length; ++i) {
    auto& entry = stack.stack()[i];
    switch (entry.type()) {
      case HangEntry::TnsCString: {
        nsresult rv = StringFrame(aCx, ret, i, entry.get_nsCString().get());
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryBufOffset: {
        uint32_t offset = entry.get_HangEntryBufOffset().index();

        // NOTE: We can't trust the offset we got, as we might have gotten it
        // from a compromised content process. Validate that it is in bounds.
        if (NS_WARN_IF(stack.strbuffer().IsEmpty() ||
                       offset >= stack.strbuffer().Length())) {
          MOZ_ASSERT_UNREACHABLE("Corrupted offset data");
          return NS_ERROR_FAILURE;
        }

        // NOTE: If our content process is compromised, it could send us back a
        // strbuffer() which didn't have a null terminator. If the last byte in
        // the buffer is not '\0', we abort, to make sure we don't read out of
        // bounds.
        if (stack.strbuffer().LastElement() != '\0') {
          MOZ_ASSERT_UNREACHABLE("Corrupted strbuffer data");
          return NS_ERROR_FAILURE;
        }

        // We know this offset is safe because of the previous checks.
        const int8_t* start = stack.strbuffer().Elements() + offset;
        nsresult rv = StringFrame(aCx, ret, i,
                                  reinterpret_cast<const char*>(start));
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryModOffset: {
        const HangEntryModOffset& mo = entry.get_HangEntryModOffset();

        JS::RootedObject jsFrame(aCx, JS_NewArrayObject(aCx, 2));
        if (!jsFrame) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, jsFrame, 0, mo.module(), JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsPrintfCString hexString("%" PRIxPTR, (uintptr_t)mo.offset());
        JS::RootedString hex(aCx, JS_NewStringCopyZ(aCx, hexString.get()));
        if (!hex || !JS_DefineElement(aCx, jsFrame, 1, hex, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!JS_DefineElement(aCx, ret, i, jsFrame, JSPROP_ENUMERATE)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        break;
      }
      case HangEntry::THangEntryProgCounter: {
        // Don't bother recording fixed program counters to JS
        nsresult rv = StringFrame(aCx, ret, i, "(unresolved)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryContent: {
        nsresult rv = StringFrame(aCx, ret, i, "(content script)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryJit: {
        nsresult rv = StringFrame(aCx, ret, i, "(jit frame)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryWasm: {
        nsresult rv = StringFrame(aCx, ret, i, "(wasm)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntryChromeScript: {
        nsresult rv = StringFrame(aCx, ret, i, "(chrome script)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case HangEntry::THangEntrySuppressed: {
        nsresult rv = StringFrame(aCx, ret, i, "(profiling suppressed)");
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      default: MOZ_CRASH("Unsupported HangEntry type?");
    }
  }

  aStack.setObject(*ret);
  return NS_OK;
}

NS_IMETHODIMP
nsHangDetails::GetModules(JSContext* aCx, JS::MutableHandleValue aVal)
{
  auto& modules = mDetails.stack().modules();
  size_t length = modules.Length();
  JS::RootedObject retObj(aCx, JS_NewArrayObject(aCx, length));
  if (!retObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < length; ++i) {
    const HangModule& module = modules[i];
    JS::RootedObject jsModule(aCx, JS_NewArrayObject(aCx, 2));
    if (!jsModule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString name(aCx, JS_NewUCStringCopyN(aCx,
                                                   module.name().BeginReading(),
                                                   module.name().Length()));
    if (!JS_DefineElement(aCx, jsModule, 0, name, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::RootedString breakpadId(aCx, JS_NewStringCopyN(aCx,
                                                       module.breakpadId().BeginReading(),
                                                       module.breakpadId().Length()));
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
        hangDetails->mDetails.remoteType().Assign(cc->GetRemoteType());
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
  });

  nsresult rv = SystemGroup::Dispatch(TaskCategory::Other,
                                      notifyObservers.forget());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMPL_ISUPPORTS(nsHangDetails, nsIHangDetails)

namespace {

// Sorting comparator used by ReadModuleInformation. Sorts PC Frames by their
// PC.
struct PCFrameComparator {
  bool LessThan(HangEntry* const& a, HangEntry* const& b) const {
    return a->get_HangEntryProgCounter().pc() < b->get_HangEntryProgCounter().pc();
  }
  bool Equals(HangEntry* const& a, HangEntry* const& b) const {
    return a->get_HangEntryProgCounter().pc() == b->get_HangEntryProgCounter().pc();
  }
};

} // anonymous namespace

void
ReadModuleInformation(HangStack& stack)
{
  // modules() should be empty when we start filling it.
  stack.modules().Clear();

#ifdef MOZ_GECKO_PROFILER
  // Create a sorted list of the PCs in the current stack.
  AutoTArray<HangEntry*, 100> frames;
  for (auto& frame : stack.stack()) {
    if (frame.type() == HangEntry::THangEntryProgCounter) {
      frames.AppendElement(&frame);
    }
  }
  PCFrameComparator comparator;
  frames.Sort(comparator);

  SharedLibraryInfo rawModules = SharedLibraryInfo::GetInfoForSelf();
  rawModules.SortByAddress();

  size_t frameIdx = 0;
  for (size_t i = 0; i < rawModules.GetSize(); ++i) {
    const SharedLibrary& info = rawModules.GetEntry(i);
    uintptr_t moduleStart = info.GetStart();
    uintptr_t moduleEnd = info.GetEnd() - 1;
    // the interval is [moduleStart, moduleEnd)

    bool moduleReferenced = false;
    for (; frameIdx < frames.Length(); ++frameIdx) {
      auto& frame = frames[frameIdx];
      uint64_t pc = frame->get_HangEntryProgCounter().pc();
      // We've moved past this frame, let's go to the next one.
      if (pc >= moduleEnd) {
        break;
      }
      if (pc >= moduleStart) {
        uint64_t offset = pc - moduleStart;
        if (NS_WARN_IF(offset > UINT32_MAX)) {
          continue; // module/offset can only hold 32-bit offsets into shared libraries.
        }

        // If we found the module, rewrite the Frame entry to instead be a
        // ModOffset one. mModules.Length() will be the index of the module when
        // we append it below, and we set moduleReferenced to true to ensure
        // that we do.
        moduleReferenced = true;
        uint32_t module = stack.modules().Length();
        HangEntryModOffset modOffset(module, static_cast<uint32_t>(offset));
        *frame = modOffset;
      }
    }

    if (moduleReferenced) {
      nsDependentCString cstr(info.GetBreakpadId().c_str());
      HangModule module(info.GetDebugName(), cstr);
      stack.modules().AppendElement(module);
    }
  }
#endif
}

NS_IMETHODIMP
ProcessHangStackRunnable::Run()
{
  // NOTE: Reading module information can take a long time, which is why we do
  // it off-main-thread.
  ReadModuleInformation(mHangDetails.stack());

  RefPtr<nsHangDetails> hangDetails = new nsHangDetails(std::move(mHangDetails));
  hangDetails->Submit();

  return NS_OK;
}

} // namespace mozilla
