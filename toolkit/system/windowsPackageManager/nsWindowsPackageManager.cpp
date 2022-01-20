/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsPackageManager.h"
#include "mozilla/Logging.h"
#include "mozilla/WindowsVersion.h"
#ifndef __MINGW32__
#  include <comutil.h>
#  include <wrl.h>
#  include <windows.management.deployment.h>
#endif  // __MINGW32__
#include "nsError.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOMCID.h"

#ifndef __MINGW32__  // WinRT headers not yet supported by MinGW
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Management;
#endif

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
  // The classes we're using are only available beginning with Windows 10
  if (!mozilla::IsWin10OrLater()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

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
  // The classes we're using are only available beginning with Windows 10
  if (!mozilla::IsWin10OrLater()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

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

}  // namespace system
}  // namespace toolkit
}  // namespace mozilla
