/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutThirdParty.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/NativeNt.h"
#include "mozilla/StaticPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIWindowsRegKey.h"
#include "nsThreadUtils.h"

#include <objbase.h>

using namespace mozilla;

namespace {

// A callback function passed to EnumSubkeys uses this type
// to control the enumeration loop.
enum class CallbackResult { Continue, Stop };

template <typename CallbackT>
void EnumSubkeys(nsIWindowsRegKey* aRegBase, const CallbackT& aCallback) {
  uint32_t count = 0;
  if (NS_FAILED(aRegBase->GetChildCount(&count))) {
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    nsAutoString subkeyName;
    if (NS_FAILED(aRegBase->GetChildName(i, subkeyName))) {
      continue;
    }

    nsCOMPtr<nsIWindowsRegKey> subkey;
    if (NS_FAILED(aRegBase->OpenChild(subkeyName, nsIWindowsRegKey::ACCESS_READ,
                                      getter_AddRefs(subkey)))) {
      continue;
    }

    CallbackResult result = aCallback(subkeyName, subkey);
    if (result == CallbackResult::Continue) {
      continue;
    } else if (result == CallbackResult::Stop) {
      break;
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected CallbackResult.");
    }
  }
}

}  // anonymous namespace

class KnownModule final {
  static KnownModule sKnownExtensions[static_cast<int>(KnownModuleType::Last)];

  static bool GetInprocServerDllPathFromGuid(const GUID& aGuid,
                                             nsAString& aResult) {
    nsAutoStringN<60> subkey;
    subkey.AppendPrintf(
        "CLSID\\{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\"
        "InProcServer32",
        aGuid.Data1, aGuid.Data2, aGuid.Data3, aGuid.Data4[0], aGuid.Data4[1],
        aGuid.Data4[2], aGuid.Data4[3], aGuid.Data4[4], aGuid.Data4[5],
        aGuid.Data4[6], aGuid.Data4[7]);

    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, subkey,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
      return false;
    }

    rv = regKey->ReadStringValue(u""_ns, aResult);
    return NS_SUCCEEDED(rv);
  }

  enum class HandlerType {
    // For this type of handler, multiple extensions can be registered as
    // subkeys under the handler subkey.
    Multi,
    // For this type of handler, a single extension can be registered as
    // the default value of the handler subkey.
    Single,
  };

  HandlerType mHandlerType;
  nsLiteralString mSubkeyName;

  using CallbackT = std::function<void(const nsString&, KnownModuleType)>;

  void EnumInternal(nsIWindowsRegKey* aRegBase, KnownModuleType aType,
                    const CallbackT& aCallback) const {
    nsCOMPtr<nsIWindowsRegKey> shexType;
    if (NS_FAILED(aRegBase->OpenChild(mSubkeyName,
                                      nsIWindowsRegKey::ACCESS_READ,
                                      getter_AddRefs(shexType)))) {
      return;
    }

    switch (mHandlerType) {
      case HandlerType::Single: {
        nsAutoString valData;
        GUID guid;
        if (NS_FAILED(shexType->ReadStringValue(u""_ns, valData)) ||
            FAILED(::CLSIDFromString(valData.get(), &guid))) {
          return;
        }

        nsAutoString dllPath;
        if (!GetInprocServerDllPathFromGuid(guid, dllPath)) {
          return;
        }

        aCallback(dllPath, aType);
        break;
      }

      case HandlerType::Multi:
        EnumSubkeys(shexType, [aType, &aCallback](const nsString& aSubKeyName,
                                                  nsIWindowsRegKey* aSubKey) {
          GUID guid;
          HRESULT hr = ::CLSIDFromString(aSubKeyName.get(), &guid);
          if (hr == CO_E_CLASSSTRING) {
            // If the key's name is not a GUID, the default value of the key
            // may be a GUID.
            nsAutoString valData;
            if (NS_SUCCEEDED(aSubKey->ReadStringValue(u""_ns, valData))) {
              hr = ::CLSIDFromString(valData.get(), &guid);
            }
          }

          if (FAILED(hr)) {
            return CallbackResult::Continue;
          }

          nsAutoString dllPath;
          if (!GetInprocServerDllPathFromGuid(guid, dllPath)) {
            return CallbackResult::Continue;
          }

          aCallback(dllPath, aType);
          return CallbackResult::Continue;
        });
        break;

      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected KnownModule::Type.");
        break;
    }
  }

  static void Enum(nsIWindowsRegKey* aRegBase, KnownModuleType aType,
                   const CallbackT& aCallback) {
    sKnownExtensions[static_cast<int>(aType)].EnumInternal(aRegBase, aType,
                                                           aCallback);
  }

  KnownModule(HandlerType aHandlerType, nsLiteralString aSubkeyName)
      : mHandlerType(aHandlerType), mSubkeyName(aSubkeyName) {}

 public:
  static void EnumAll(const CallbackT& aCallback) {
    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    // Icon Overlay Handlers are registered under HKLM only.
    // No need to look at HKCU.
    rv = regKey->Open(
        nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
        u"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"_ns,
        nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      Enum(regKey, KnownModuleType::IconOverlay, aCallback);
    }

    // IMEs can be enumerated by
    // ITfInputProcessorProfiles::EnumInputProcessorInfo, but enumerating
    // the registry key is easier.
    // The "HKLM\Software\Microsoft\CTF\TIP" subtree is shared between
    // the 32-bits and 64 bits views.
    // https://docs.microsoft.com/en-us/windows/win32/winprog64/shared-registry-keys
    // This logic cannot detect legacy (TSF-unaware) IMEs.
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      u"Software\\Microsoft\\CTF"_ns,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      Enum(regKey, KnownModuleType::Ime, aCallback);
    }

    // Because HKCR is a merged view of HKLM\Software\Classes and
    // HKCU\Software\Classes, looking at HKCR covers both per-machine
    // and per-user extensions.
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, u""_ns,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
      return;
    }

    EnumSubkeys(regKey, [&aCallback](const nsString& aSubKeyName,
                                     nsIWindowsRegKey* aSubKey) {
      if (aSubKeyName.EqualsIgnoreCase("DesktopBackground") ||
          aSubKeyName.EqualsIgnoreCase("AudioCD")) {
        return CallbackResult::Continue;
      }

      if (aSubKeyName.EqualsIgnoreCase("Directory")) {
        nsCOMPtr<nsIWindowsRegKey> regBackground;
        if (NS_SUCCEEDED(aSubKey->OpenChild(u"Background\\shellex"_ns,
                                            nsIWindowsRegKey::ACCESS_READ,
                                            getter_AddRefs(regBackground)))) {
          Enum(regBackground, KnownModuleType::ContextMenuHandler, aCallback);
        }
      } else if (aSubKeyName.EqualsIgnoreCase("Network")) {
        nsCOMPtr<nsIWindowsRegKey> regNetworkTypes;
        if (NS_SUCCEEDED(aSubKey->OpenChild(u"Type"_ns,
                                            nsIWindowsRegKey::ACCESS_READ,
                                            getter_AddRefs(regNetworkTypes)))) {
          EnumSubkeys(
              regNetworkTypes,
              [&aCallback](const nsString&, nsIWindowsRegKey* aRegNetworkType) {
                nsCOMPtr<nsIWindowsRegKey> regNetworkTypeShex;
                if (NS_FAILED(aRegNetworkType->OpenChild(
                        u"shellex"_ns, nsIWindowsRegKey::ACCESS_READ,
                        getter_AddRefs(regNetworkTypeShex)))) {
                  return CallbackResult::Continue;
                }

                Enum(regNetworkTypeShex, KnownModuleType::ContextMenuHandler,
                     aCallback);
                Enum(regNetworkTypeShex, KnownModuleType::PropertySheetHandler,
                     aCallback);
                return CallbackResult::Continue;
              });
        }
      }

      nsCOMPtr<nsIWindowsRegKey> regShex;
      if (NS_FAILED(aSubKey->OpenChild(u"shellex"_ns,
                                       nsIWindowsRegKey::ACCESS_READ,
                                       getter_AddRefs(regShex)))) {
        return CallbackResult::Continue;
      }

      Enum(regShex, KnownModuleType::ContextMenuHandler, aCallback);
      Enum(regShex, KnownModuleType::PropertySheetHandler, aCallback);

      if (aSubKeyName.EqualsIgnoreCase("AllFileSystemObjects") ||
          aSubKeyName.EqualsIgnoreCase("Network") ||
          aSubKeyName.EqualsIgnoreCase("NetShare") ||
          aSubKeyName.EqualsIgnoreCase("NetServer") ||
          aSubKeyName.EqualsIgnoreCase("DVD")) {
        return CallbackResult::Continue;
      }

      if (aSubKeyName.EqualsIgnoreCase("Directory")) {
        Enum(regShex, KnownModuleType::CopyHookHandler, aCallback);
        Enum(regShex, KnownModuleType::DragDropHandler, aCallback);
        return CallbackResult::Continue;
      } else if (aSubKeyName.EqualsIgnoreCase("Drive")) {
        Enum(regShex, KnownModuleType::DragDropHandler, aCallback);
        return CallbackResult::Continue;
      } else if (aSubKeyName.EqualsIgnoreCase("Folder")) {
        Enum(regShex, KnownModuleType::DragDropHandler, aCallback);
        return CallbackResult::Continue;
      } else if (aSubKeyName.EqualsIgnoreCase("Printers")) {
        Enum(regShex, KnownModuleType::CopyHookHandler, aCallback);
        return CallbackResult::Continue;
      }

      Enum(regShex, KnownModuleType::DataHandler, aCallback);
      Enum(regShex, KnownModuleType::DropHandler, aCallback);
      Enum(regShex, KnownModuleType::IconHandler, aCallback);
      Enum(regShex, KnownModuleType::PropertyHandler, aCallback);
      Enum(regShex, KnownModuleType::InfotipHandler, aCallback);
      return CallbackResult::Continue;
    });
  }

  KnownModule() = delete;
  KnownModule(KnownModule&&) = delete;
  KnownModule& operator=(KnownModule&&) = delete;
  KnownModule(const KnownModule&) = delete;
  KnownModule& operator=(const KnownModule&) = delete;
};

KnownModule KnownModule::sKnownExtensions[] = {
    {HandlerType::Multi, u"TIP"_ns},
    {HandlerType::Multi, u"ShellIconOverlayIdentifiers"_ns},
    {HandlerType::Multi, u"ContextMenuHandlers"_ns},
    {HandlerType::Multi, u"CopyHookHandlers"_ns},
    {HandlerType::Multi, u"DragDropHandlers"_ns},
    {HandlerType::Multi, u"PropertySheetHandlers"_ns},
    {HandlerType::Single, u"DataHandler"_ns},
    {HandlerType::Single, u"DropHandler"_ns},
    {HandlerType::Single, u"IconHandler"_ns},
    {HandlerType::Single, u"{00021500-0000-0000-C000-000000000046}"_ns},
    {HandlerType::Single, u"PropertyHandler"_ns},
};

namespace mozilla {

static StaticRefPtr<AboutThirdParty> sSingleton;

NS_IMPL_ISUPPORTS(AboutThirdParty, nsIAboutThirdParty);

/*static*/
already_AddRefed<AboutThirdParty> AboutThirdParty::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new AboutThirdParty;
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

AboutThirdParty::AboutThirdParty()
    : mPromise(new BackgroundThreadPromise::Private(__func__)) {}

void AboutThirdParty::AddKnownModule(const nsString& aPath,
                                     KnownModuleType aType) {
  MOZ_ASSERT(!NS_IsMainThread());

  const uint32_t flag = 1u << static_cast<uint32_t>(aType);
  mKnownModules.WithEntryHandle(nt::GetLeafName(aPath), [flag](auto&& addPtr) {
    if (addPtr) {
      addPtr.Data() |= flag;
    } else {
      addPtr.Insert(flag);
    }
  });
}

void AboutThirdParty::BackgroundThread() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mWorkerState == WorkerState::Running);

  KnownModule::EnumAll(
      [self = RefPtr{this}](const nsString& aDllPath, KnownModuleType aType) {
        self->AddKnownModule(aDllPath, aType);
      });

  mWorkerState = WorkerState::Done;
}

NS_IMETHODIMP AboutThirdParty::LookupModuleType(const nsAString& aLeafName,
                                                uint32_t* aResult) {
  constexpr uint32_t kShellExtensions =
      1u << static_cast<uint32_t>(KnownModuleType::IconOverlay) |
      1u << static_cast<uint32_t>(KnownModuleType::ContextMenuHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::CopyHookHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::DragDropHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::PropertySheetHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::DataHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::DropHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::IconHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::InfotipHandler) |
      1u << static_cast<uint32_t>(KnownModuleType::PropertyHandler);

  MOZ_ASSERT(NS_IsMainThread());

  *aResult = 0;
  if (mWorkerState != WorkerState::Done) {
    return NS_OK;
  }

  uint32_t flags;
  if (!mKnownModules.Get(aLeafName, &flags)) {
    *aResult = nsIAboutThirdParty::ModuleType_Unknown;
    return NS_OK;
  }

  if (flags & (1u << static_cast<uint32_t>(KnownModuleType::Ime))) {
    *aResult |= nsIAboutThirdParty::ModuleType_IME;
  }

  if (flags & kShellExtensions) {
    *aResult |= nsIAboutThirdParty::ModuleType_ShellExtension;
  }

  return NS_OK;
}

RefPtr<BackgroundThreadPromise> AboutThirdParty::CollectSystemInfoAsync() {
  MOZ_ASSERT(NS_IsMainThread());

  // Allow only the first call to start a background task.
  if (mWorkerState.compareExchange(WorkerState::Init, WorkerState::Running)) {
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "AboutThirdParty::BackgroundThread", [self = RefPtr{this}]() mutable {
          self->BackgroundThread();
          NS_DispatchToMainThread(NS_NewRunnableFunction(
              "AboutThirdParty::BackgroundThread Done",
              [self]() { self->mPromise->Resolve(true, __func__); }));
        });

    nsresult rv =
        NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      mPromise->Reject(rv, __func__);
    }
  }

  return mPromise;
}

NS_IMETHODIMP
AboutThirdParty::CollectSystemInfo(JSContext* aCx, dom::Promise** aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);

  ErrorResult result;
  RefPtr<dom::Promise> promise(dom::Promise::Create(global, result));
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  CollectSystemInfoAsync()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](bool) { promise->MaybeResolve(JS::NullHandleValue); },
      [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aResult);
  return NS_OK;
}

}  // namespace mozilla
