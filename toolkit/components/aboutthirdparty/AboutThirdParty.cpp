/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutThirdParty.h"

#include "AboutThirdPartyUtils.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/DynamicBlocklist.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/NativeNt.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WinDllServices.h"
#include "MsiDatabase.h"
#include "nsAppRunner.h"
#include "nsComponentManagerUtils.h"
#include "nsIWindowsRegKey.h"
#include "nsThreadUtils.h"

#include <objbase.h>

using namespace mozilla;

template <>
bool DllBlockInfo::IsValidDynamicBlocklistEntry() const {
  if (!mName.Buffer || !mName.Length || mName.Length > mName.MaximumLength) {
    return false;
  }
  MOZ_ASSERT(mMaxVersion == DllBlockInfo::ALL_VERSIONS,
             "dynamic blocklist does not allow custom version");
  MOZ_ASSERT(mFlags == DllBlockInfoFlags::FLAGS_DEFAULT,
             "dynamic blocklist does not allow custom flags");
  return true;
}

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

Span<const DllBlockInfo> GetDynamicBlocklistSpan(
    RefPtr<DllServices>&& aDllSvc) {
  if (!aDllSvc) {
    return nullptr;
  }

  nt::SharedSection* sharedSection = aDllSvc->GetSharedSection();
  if (!sharedSection) {
    return nullptr;
  }

  return sharedSection->GetDynamicBlocklist();
}

}  // anonymous namespace

InstallLocationComparator::InstallLocationComparator(const nsAString& aFilePath)
    : mFilePath(aFilePath) {}

int InstallLocationComparator::operator()(
    const InstallLocationT& aLocation) const {
  // Firstly we check whether mFilePath begins with aLocation.
  // If yes, mFilePath is a part of the target installation,
  // so we return 0 showing match.
  const nsAString& location = aLocation.first();
  size_t locationLen = location.Length();
  if (locationLen <= mFilePath.Length() &&
      nsCaseInsensitiveStringComparator(mFilePath.BeginReading(),
                                        location.BeginReading(), locationLen,
                                        locationLen) == 0) {
    return 0;
  }

  return CompareIgnoreCase(mFilePath, location);
}

// The InstalledApplications class behaves like Chrome's InstalledApplications,
// which collects installed applications from two resources below.
//
// 1) Path strings in MSI package components
// An MSI package is consisting of multiple components.  This class collects
// MSI components representing a file and stores them as a hash table.
//
// 2) Install location paths in the InstallLocation registry value
// If an application's installer is not MSI but sets the InstallLocation
// registry value, we can use it to search for an application by comparing
// a target module is located under that location path.  This class stores
// location path strings as a sorted array so that we can binary-search it.
class InstalledApplications final {
  // Limit the number of entries to avoid consuming too much memory
  constexpr static uint32_t kMaxComponents = 1000000;
  constexpr static uint32_t kMaxInstallLocations = 1000;

  nsCOMPtr<nsIWindowsRegKey> mInstallerData;
  nsCOMPtr<nsIInstalledApplication> mCurrentApp;
  ComponentPathMapT mComponentPaths;
  nsTArray<InstallLocationT> mLocations;

  void AddInstallLocation(nsIWindowsRegKey* aProductSubKey) {
    nsString location;
    if (NS_FAILED(
            aProductSubKey->ReadStringValue(u"InstallLocation"_ns, location)) ||
        location.IsEmpty()) {
      return;
    }

    if (location.Last() != u'\\') {
      location.Append(u'\\');
    }

    mLocations.EmplaceBack(location, this->mCurrentApp);
  }

  void AddComponentGuid(const nsString& aPackedProductGuid,
                        const nsString& aPackedComponentGuid) {
    nsAutoString componentSubkey(L"Components\\");
    componentSubkey += aPackedComponentGuid;

    // Pick a first value in the subkeys under |componentSubkey|.
    nsString componentPath;

    EnumSubkeys(mInstallerData, [&aPackedProductGuid, &componentSubkey,
                                 &componentPath](const nsString& aSid,
                                                 nsIWindowsRegKey* aSidSubkey) {
      // If we have a value in |componentPath|, the loop should
      // have been stopped.
      MOZ_ASSERT(componentPath.IsEmpty());

      nsCOMPtr<nsIWindowsRegKey> compKey;
      nsresult rv =
          aSidSubkey->OpenChild(componentSubkey, nsIWindowsRegKey::ACCESS_READ,
                                getter_AddRefs(compKey));
      if (NS_FAILED(rv)) {
        return CallbackResult::Continue;
      }

      nsString compData;
      if (NS_FAILED(compKey->ReadStringValue(aPackedProductGuid, compData))) {
        return CallbackResult::Continue;
      }

      if (!CorrectMsiComponentPath(compData)) {
        return CallbackResult::Continue;
      }

      componentPath = std::move(compData);
      return CallbackResult::Stop;
    });

    if (componentPath.IsEmpty()) {
      return;
    }

    // Use a full path as a key rather than a leaf name because
    // the same name's module can be installed under system32
    // and syswow64.
    mComponentPaths.WithEntryHandle(componentPath, [this](auto&& addPtr) {
      if (addPtr) {
        // If the same file appeared in multiple installations, we set null
        // for its value because there is no way to know which installation is
        // the real owner.
        addPtr.Data() = nullptr;
      } else {
        addPtr.Insert(this->mCurrentApp);
      }
    });
  }

  void AddProduct(const nsString& aProductId,
                  nsIWindowsRegKey* aProductSubKey) {
    nsString displayName;
    if (NS_FAILED(
            aProductSubKey->ReadStringValue(u"DisplayName"_ns, displayName)) ||
        displayName.IsEmpty()) {
      // Skip if no name is found.
      return;
    }

    nsString publisher;
    if (NS_SUCCEEDED(
            aProductSubKey->ReadStringValue(u"Publisher"_ns, publisher)) &&
        publisher.EqualsIgnoreCase("Microsoft") &&
        publisher.EqualsIgnoreCase("Microsoft Corporation")) {
      // Skip if the publisher is Microsoft because it's not a third-party.
      // We don't skip an application without the publisher name.
      return;
    }

    mCurrentApp =
        new InstalledApplication(std::move(displayName), std::move(publisher));
    // Try an MSI database first because it's more accurate,
    // then fall back to the InstallLocation key.
    do {
      if (!mInstallerData) {
        break;
      }

      nsAutoString packedProdGuid;
      if (!MsiPackGuid(aProductId, packedProdGuid)) {
        break;
      }

      auto db = MsiDatabase::FromProductId(aProductId.get());
      if (db.isNothing()) {
        break;
      }

      db->ExecuteSingleColumnQuery(
          L"SELECT DISTINCT ComponentId FROM Component",
          [this, &packedProdGuid](const wchar_t* aComponentGuid) {
            if (this->mComponentPaths.Count() >= kMaxComponents) {
              return MsiDatabase::CallbackResult::Stop;
            }

            nsAutoString packedComponentGuid;
            if (MsiPackGuid(nsDependentString(aComponentGuid),
                            packedComponentGuid)) {
              this->AddComponentGuid(packedProdGuid, packedComponentGuid);
            }

            return MsiDatabase::CallbackResult::Continue;
          });

      // We've decided to collect data from an MSI database.
      // Exiting the function.
      return;
    } while (false);

    if (mLocations.Length() >= kMaxInstallLocations) {
      return;
    }

    // If we cannot use an MSI database for any reason,
    // try the InstallLocation key.
    AddInstallLocation(aProductSubKey);
  }

 public:
  InstalledApplications() {
    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_SUCCEEDED(rv) &&
        NS_SUCCEEDED(regKey->Open(
            nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
            u"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
            u"Installer\\UserData"_ns,
            nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::WOW64_64))) {
      mInstallerData.swap(regKey);
    }
  }
  ~InstalledApplications() = default;

  InstalledApplications(InstalledApplications&&) = delete;
  InstalledApplications& operator=(InstalledApplications&&) = delete;
  InstalledApplications(const InstalledApplications&) = delete;
  InstalledApplications& operator=(const InstalledApplications&) = delete;

  void Collect(ComponentPathMapT& aOutComponentPaths,
               nsTArray<InstallLocationT>& aOutLocations) {
    const nsLiteralString kUninstallKey(
        u"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    static const uint16_t sProcessor = []() -> uint16_t {
      SYSTEM_INFO si;
      ::GetSystemInfo(&si);
      return si.wProcessorArchitecture;
    }();

    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    switch (sProcessor) {
      case PROCESSOR_ARCHITECTURE_INTEL:
        rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                          kUninstallKey, nsIWindowsRegKey::ACCESS_READ);
        if (NS_SUCCEEDED(rv)) {
          EnumSubkeys(regKey, [this](const nsString& aProductId,
                                     nsIWindowsRegKey* aProductSubKey) {
            this->AddProduct(aProductId, aProductSubKey);
            return CallbackResult::Continue;
          });
        }
        break;

      case PROCESSOR_ARCHITECTURE_AMD64:
        // A 64-bit application may be installed by a 32-bit installer,
        // or vice versa.  So we enumerate both views regardless of
        // the process's (not processor's) bitness.
        rv = regKey->Open(
            nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, kUninstallKey,
            nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::WOW64_64);
        if (NS_SUCCEEDED(rv)) {
          EnumSubkeys(regKey, [this](const nsString& aProductId,
                                     nsIWindowsRegKey* aProductSubKey) {
            this->AddProduct(aProductId, aProductSubKey);
            return CallbackResult::Continue;
          });
        }
        rv = regKey->Open(
            nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, kUninstallKey,
            nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::WOW64_32);
        if (NS_SUCCEEDED(rv)) {
          EnumSubkeys(regKey, [this](const nsString& aProductId,
                                     nsIWindowsRegKey* aProductSubKey) {
            this->AddProduct(aProductId, aProductSubKey);
            return CallbackResult::Continue;
          });
        }
        break;

      default:
        MOZ_ASSERT(false, "Unsupported CPU architecture");
        return;
    }

    // The "HKCU\SOFTWARE\" subtree is shared between the 32-bits and 64 bits
    // views.  No need to enumerate wow6432node for HKCU.
    // https://docs.microsoft.com/en-us/windows/win32/winprog64/shared-registry-keys
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, kUninstallKey,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      EnumSubkeys(regKey, [this](const nsString& aProductId,
                                 nsIWindowsRegKey* aProductSubKey) {
        this->AddProduct(aProductId, aProductSubKey);
        return CallbackResult::Continue;
      });
    }

    aOutComponentPaths.SwapElements(mComponentPaths);

    mLocations.Sort([](const InstallLocationT& aA, const InstallLocationT& aB) {
      return CompareIgnoreCase(aA.first(), aB.first());
    });
    aOutLocations.SwapElements(mLocations);
  }
};

class KnownModule final {
  static KnownModule sKnownExtensions[static_cast<int>(KnownModuleType::Last)];

  static bool GetInprocServerDllPathFromGuid(const GUID& aGuid,
                                             nsAString& aResult) {
    nsAutoStringN<60> subkey;
    subkey.AppendPrintf(
        "CLSID\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\"
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

NS_IMPL_ISUPPORTS(InstalledApplication, nsIInstalledApplication);
NS_IMPL_ISUPPORTS(AboutThirdParty, nsIAboutThirdParty);

InstalledApplication::InstalledApplication(nsString&& aAppName,
                                           nsString&& aPublisher)
    : mName(std::move(aAppName)), mPublisher(std::move(aPublisher)) {}

NS_IMETHODIMP
InstalledApplication::GetName(nsAString& aResult) {
  aResult = mName;
  return NS_OK;
}

NS_IMETHODIMP
InstalledApplication::GetPublisher(nsAString& aResult) {
  aResult = mPublisher;
  return NS_OK;
}

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

  auto cleanup = MakeScopeExit(
      [self = RefPtr{this}] { self->mWorkerState = WorkerState::Done; });

  RefPtr<DllServices> dllSvc(DllServices::Get());
  if (!dllSvc) {
    // Probably we're shutting down.  Bail out before expensive tasks.
    return;
  }

  KnownModule::EnumAll(
      [self = RefPtr{this}](const nsString& aDllPath, KnownModuleType aType) {
        self->AddKnownModule(aDllPath, aType);
      });

  InstalledApplications apps;
  apps.Collect(mComponentPaths, mLocations);

#if defined(MOZ_LAUNCHER_PROCESS)
  Span<const DllBlockInfo> blocklist =
      GetDynamicBlocklistSpan(std::move(dllSvc));
  for (const auto& info : blocklist) {
    if (!info.IsValidDynamicBlocklistEntry()) {
      break;
    }

    nsString name(info.mName.Buffer, info.mName.Length / sizeof(wchar_t));
    mDynamicBlocklist.Insert(name);
    mDynamicBlocklistAtLaunch.Insert(std::move(name));
  }
#endif  // defined(MOZ_LAUNCHER_PROCESS)
}

NS_IMETHODIMP AboutThirdParty::LookupModuleType(const nsAString& aLeafName,
                                                uint32_t* aResult) {
  static_assert(static_cast<uint32_t>(KnownModuleType::Last) <= 32,
                "Too many flags in KnownModuleType");
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

#if defined(MOZ_LAUNCHER_PROCESS)
  if (mDynamicBlocklist.Contains(aLeafName)) {
    *aResult |= nsIAboutThirdParty::ModuleType_BlockedByUser;
  }
  if (mDynamicBlocklistAtLaunch.Contains(aLeafName)) {
    *aResult |= nsIAboutThirdParty::ModuleType_BlockedByUserAtLaunch;
  }
#endif

  uint32_t flags;
  if (!mKnownModules.Get(aLeafName, &flags)) {
    *aResult |= nsIAboutThirdParty::ModuleType_Unknown;
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

NS_IMETHODIMP AboutThirdParty::LookupApplication(
    const nsAString& aModulePath, nsIInstalledApplication** aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  *aResult = nullptr;
  if (mWorkerState != WorkerState::Done) {
    return NS_OK;
  }

  const nsDependentSubstring leaf = nt::GetLeafName(aModulePath);
  if (leaf.IsEmpty()) {
    return NS_OK;
  }

  // Look up the component path's map first because it's more accurate
  // than the location's array.
  nsCOMPtr<nsIInstalledApplication> app = mComponentPaths.Get(aModulePath);
  if (app) {
    app.forget(aResult);
    return NS_OK;
  }

  auto bounds = EqualRange(mLocations, 0, mLocations.Length(),
                           InstallLocationComparator(aModulePath));

  // If more than one application includes the module, we return null
  // because there is no way to know which is the real owner.
  if (bounds.second - bounds.first != 1) {
    return NS_OK;
  }

  app = mLocations[bounds.first].second();
  app.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP AboutThirdParty::GetIsDynamicBlocklistAvailable(
    bool* aIsDynamicBlocklistAvailable) {
  *aIsDynamicBlocklistAvailable =
      !GetDynamicBlocklistSpan(DllServices::Get()).IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP AboutThirdParty::GetIsDynamicBlocklistDisabled(
    bool* aIsDynamicBlocklistDisabled) {
  *aIsDynamicBlocklistDisabled = IsDynamicBlocklistDisabled(
      gSafeMode, CommandLine::ForCurrentProcess()->HasSwitch(UTF8ToWide(
                     mozilla::geckoargs::sDisableDynamicDllBlocklist.sMatch)));
  return NS_OK;
}

NS_IMETHODIMP AboutThirdParty::UpdateBlocklist(const nsAString& aLeafName,
                                               bool aNewBlockStatus,
                                               JSContext* aCx,
                                               dom::Promise** aResult) {
#if defined(MOZ_LAUNCHER_PROCESS)
  MOZ_ASSERT(NS_IsMainThread());

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);

  ErrorResult result;
  RefPtr<dom::Promise> promise(dom::Promise::Create(global, result));
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  auto returnPromise = MakeScopeExit([&] { promise.forget(aResult); });

  if (aNewBlockStatus) {
    mDynamicBlocklist.Insert(aLeafName);
  } else {
    mDynamicBlocklist.Remove(aLeafName);
  }

  auto newTask = MakeUnique<DynamicBlocklistWriter>(promise, mDynamicBlocklist);
  if (!newTask->IsReady()) {
    promise->MaybeReject(NS_ERROR_CANNOT_CONVERT_DATA);
    return NS_OK;
  }

  UniquePtr<DynamicBlocklistWriter> oldTask(
      mPendingWriter.exchange(newTask.release()));
  if (oldTask) {
    oldTask->Cancel();
  }

  nsresult rv = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(__func__,
                             [self = RefPtr{this}]() {
                               UniquePtr<DynamicBlocklistWriter> task(
                                   self->mPendingWriter.exchange(nullptr));
                               if (task) {
                                 task->Run();
                               }
                             }),
      NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
  }
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif  // defined(MOZ_LAUNCHER_PROCESS)
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
