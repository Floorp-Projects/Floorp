/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsPackageManager.h"
#include "mozilla/Logging.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/mscom/EnsureMTA.h"
#ifndef __MINGW32__
#  include <comutil.h>
#  include <wrl.h>
#  include <windows.applicationmodel.store.h>
#  include <windows.management.deployment.h>
#  include <windows.services.store.h>
#endif  // __MINGW32__
#include "nsError.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOMCID.h"
#include "json/json.h"

#ifndef __MINGW32__  // WinRT headers not yet supported by MinGW
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Management;
using namespace ABI::Windows::Services::Store;
#endif

// Campaign IDs are stored in a JSON data structure under this key
// for installs done without the user being signed in to the Microsoft
// store.
#define CAMPAIGN_ID_JSON_FIELD_NAME "customPolicyField1"

namespace mozilla {
namespace toolkit {
namespace system {

NS_IMPL_ISUPPORTS(nsWindowsPackageManager, nsIWindowsPackageManager)

NS_IMETHODIMP
nsWindowsPackageManager::FindUserInstalledPackages(
    const nsTArray<nsString>& aNamePrefixes, nsTArray<nsString>& aPackages) {
#ifdef __MINGW32__
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  ComPtr<IInspectable> pmInspectable;
  ComPtr<Deployment::IPackageManager> pm;
  HRESULT hr = RoActivateInstance(
      HStringReference(
          RuntimeClass_Windows_Management_Deployment_PackageManager)
          .Get(),
      &pmInspectable);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }
  hr = pmInspectable.As(&pm);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }

  ComPtr<Collections::IIterable<ApplicationModel::Package*> > pkgs;
  hr = pm->FindPackagesByUserSecurityId(NULL, &pkgs);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }
  ComPtr<Collections::IIterator<ApplicationModel::Package*> > iterator;
  hr = pkgs->First(&iterator);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }
  boolean hasCurrent;
  hr = iterator->get_HasCurrent(&hasCurrent);
  while (SUCCEEDED(hr) && hasCurrent) {
    ComPtr<ApplicationModel::IPackage> package;
    hr = iterator->get_Current(&package);
    if (!SUCCEEDED(hr)) {
      return NS_ERROR_FAILURE;
    }
    ComPtr<ApplicationModel::IPackageId> packageId;
    hr = package->get_Id(&packageId);
    if (!SUCCEEDED(hr)) {
      return NS_ERROR_FAILURE;
    }
    HString rawName;
    hr = packageId->get_FamilyName(rawName.GetAddressOf());
    if (!SUCCEEDED(hr)) {
      return NS_ERROR_FAILURE;
    }
    unsigned int tmp;
    nsString name(rawName.GetRawBuffer(&tmp));
    for (uint32_t i = 0; i < aNamePrefixes.Length(); i++) {
      if (name.Find(aNamePrefixes.ElementAt(i)) != kNotFound) {
        aPackages.AppendElement(name);
        break;
      }
    }
    hr = iterator->MoveNext(&hasCurrent);
  }
  return NS_OK;
#endif  // __MINGW32__
}

NS_IMETHODIMP
nsWindowsPackageManager::GetInstalledDate(uint64_t* ts) {
#ifdef __MINGW32__
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  ComPtr<ApplicationModel::IPackageStatics> pkgStatics;
  HRESULT hr = RoGetActivationFactory(
      HStringReference(RuntimeClass_Windows_ApplicationModel_Package).Get(),
      IID_PPV_ARGS(&pkgStatics));
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }

  ComPtr<ApplicationModel::IPackage> package;
  hr = pkgStatics->get_Current(&package);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }

  ComPtr<ApplicationModel::IPackage3> package3;
  hr = package.As(&package3);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }

  DateTime installedDate;
  hr = package3->get_InstalledDate(&installedDate);
  if (!SUCCEEDED(hr)) {
    return NS_ERROR_FAILURE;
  }

  *ts = installedDate.UniversalTime;
  return NS_OK;
#endif  // __MINGW32__
}

NS_IMETHODIMP
nsWindowsPackageManager::GetCampaignId(nsAString& aCampaignId) {
#ifdef __MINGW32__
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  // This is only relevant for MSIX packaged builds.
  if (!mozilla::HasPackageIdentity()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  ComPtr<IStoreContextStatics> scStatics = nullptr;
  HRESULT hr = RoGetActivationFactory(
      HStringReference(RuntimeClass_Windows_Services_Store_StoreContext).Get(),
      IID_PPV_ARGS(&scStatics));
  /* Per
   * https://docs.microsoft.com/en-us/windows/uwp/publish/create-a-custom-app-promotion-campaign#programmatically-retrieve-the-custom-campaign-id-for-an-app
   * there are three ways to find a campaign ID.
   * 1) If the user was logged into the Microsoft Store when they installed
   *    it will be available in a SKU.
   * 2) If they were not logged in, it will be available through the
   *    StoreAppLicense
   *
   * There's also a third way, in theory, to retrieve it on very old versions of
   * Windows 10 (1511 or earlier). However, these versions don't appear to be
   * able to use the Windows Store at all anymore - so there's no point in
   * supporting that scenario.
   *
   */
  if (!SUCCEEDED(hr) || scStatics == nullptr) return NS_ERROR_FAILURE;
  ComPtr<IStoreContext> storeContext = nullptr;
  hr = scStatics->GetDefault(&storeContext);
  if (!SUCCEEDED(hr) || storeContext == nullptr) return NS_ERROR_FAILURE;

  ComPtr<IAsyncOperation<StoreProductResult*> > asyncSpr = nullptr;

  {
    nsAutoHandle event(CreateEventW(nullptr, true, false, nullptr));
    bool asyncOpSucceeded = false;

    // Despite the documentation indicating otherwise, the async operations
    // and callbacks used here don't seem to work outside of a COM MTA.
    mozilla::mscom::EnsureMTA(
        [&event, &asyncOpSucceeded, &hr, &storeContext, &asyncSpr]() -> void {
          auto callback =
              Callback<IAsyncOperationCompletedHandler<StoreProductResult*> >(
                  [&asyncOpSucceeded, &event](
                      IAsyncOperation<StoreProductResult*>* asyncInfo,
                      AsyncStatus status) -> HRESULT {
                    asyncOpSucceeded = status == AsyncStatus::Completed;
                    return SetEvent(event.get());
                  });

          hr = storeContext->GetStoreProductForCurrentAppAsync(&asyncSpr);
          if (!SUCCEEDED(hr) || asyncSpr == nullptr) {
            asyncOpSucceeded = false;
            return;
          }
          hr = asyncSpr->put_Completed(callback.Get());
          if (!SUCCEEDED(hr)) {
            asyncOpSucceeded = false;
            return;
          }

          DWORD ret = WaitForSingleObject(event.get(), 30000);
          if (ret != WAIT_OBJECT_0) {
            asyncOpSucceeded = false;
          }
        });
    if (!asyncOpSucceeded) return NS_ERROR_FAILURE;
  }

  ComPtr<IStoreProductResult> productResult = nullptr;
  hr = asyncSpr->GetResults(&productResult);
  if (!SUCCEEDED(hr) || productResult == nullptr) return NS_ERROR_FAILURE;

  ComPtr<IStoreProduct> product = nullptr;
  hr = productResult->get_Product(&product);
  if (!SUCCEEDED(hr) || product == nullptr) return NS_ERROR_FAILURE;
  ComPtr<Collections::IVectorView<StoreSku*> > skus = nullptr;
  hr = product->get_Skus(&skus);
  if (!SUCCEEDED(hr) || skus == nullptr) return NS_ERROR_FAILURE;

  unsigned int size;
  hr = skus->get_Size(&size);
  if (!SUCCEEDED(hr)) return NS_ERROR_FAILURE;

  for (unsigned int i = 0; i < size; i++) {
    ComPtr<IStoreSku> sku = nullptr;
    hr = skus->GetAt(i, &sku);
    if (!SUCCEEDED(hr) || sku == nullptr) return NS_ERROR_FAILURE;

    boolean isInUserCollection = false;
    hr = sku->get_IsInUserCollection(&isInUserCollection);
    if (!SUCCEEDED(hr) || !isInUserCollection) continue;

    ComPtr<IStoreCollectionData> scd = nullptr;
    hr = sku->get_CollectionData(&scd);
    if (!SUCCEEDED(hr) || scd == nullptr) continue;

    HString campaignId;
    hr = scd->get_CampaignId(campaignId.GetAddressOf());
    if (!SUCCEEDED(hr)) continue;

    unsigned int tmp;
    aCampaignId.Assign(campaignId.GetRawBuffer(&tmp));
    if (aCampaignId.Length() > 0) {
      aCampaignId.AppendLiteral("&msstoresignedin=true");
    }
  }

  // There's various points above that could exit without a failure.
  // If we get here without a campaignId we may as well just check
  // the AppStoreLicense.
  if (aCampaignId.IsEmpty()) {
    ComPtr<IAsyncOperation<StoreAppLicense*> > asyncSal = nullptr;
    bool asyncOpSucceeded = false;
    nsAutoHandle event(CreateEventW(nullptr, true, false, nullptr));
    mozilla::mscom::EnsureMTA(
        [&event, &asyncOpSucceeded, &hr, &storeContext, &asyncSal]() -> void {
          auto callback =
              Callback<IAsyncOperationCompletedHandler<StoreAppLicense*> >(
                  [&asyncOpSucceeded, &event](
                      IAsyncOperation<StoreAppLicense*>* asyncInfo,
                      AsyncStatus status) -> HRESULT {
                    asyncOpSucceeded = status == AsyncStatus::Completed;
                    return SetEvent(event.get());
                  });

          hr = storeContext->GetAppLicenseAsync(&asyncSal);
          if (!SUCCEEDED(hr) || asyncSal == nullptr) {
            asyncOpSucceeded = false;
            return;
          }
          hr = asyncSal->put_Completed(callback.Get());
          if (!SUCCEEDED(hr)) {
            asyncOpSucceeded = false;
            return;
          }

          DWORD ret = WaitForSingleObject(event.get(), 30000);
          if (ret != WAIT_OBJECT_0) {
            asyncOpSucceeded = false;
          }
        });
    if (!asyncOpSucceeded) return NS_ERROR_FAILURE;

    ComPtr<IStoreAppLicense> license = nullptr;
    hr = asyncSal->GetResults(&license);
    if (!SUCCEEDED(hr) || license == nullptr) return NS_ERROR_FAILURE;

    HString extendedData;
    hr = license->get_ExtendedJsonData(extendedData.GetAddressOf());
    if (!SUCCEEDED(hr)) return NS_ERROR_FAILURE;

    Json::Value jsonData;
    Json::Reader jsonReader;

    unsigned int tmp;
    nsAutoString key(extendedData.GetRawBuffer(&tmp));
    if (!jsonReader.parse(NS_ConvertUTF16toUTF8(key).get(), jsonData, false)) {
      return NS_ERROR_FAILURE;
    }

    if (jsonData.isMember(CAMPAIGN_ID_JSON_FIELD_NAME) &&
        jsonData[CAMPAIGN_ID_JSON_FIELD_NAME].isString()) {
      aCampaignId.Assign(
          NS_ConvertUTF8toUTF16(
              jsonData[CAMPAIGN_ID_JSON_FIELD_NAME].asString().c_str())
              .get());
      if (aCampaignId.Length() > 0) {
        aCampaignId.AppendLiteral("&msstoresignedin=false");
      }
    }
  }

  // No matter what happens in either block above, if they don't exit with a
  // failure we managed to successfully pull the campaignId from somewhere
  // (even if its empty).
  return NS_OK;

#endif  // __MINGW32__
}

}  // namespace system
}  // namespace toolkit
}  // namespace mozilla
