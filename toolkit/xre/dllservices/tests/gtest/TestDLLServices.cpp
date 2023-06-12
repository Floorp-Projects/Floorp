/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/WinDllServices.h"
#include "nsDirectoryServiceDefs.h"

using namespace mozilla;

static nsString GetFullDllPath(const nsAString& aLeaf) {
  nsCOMPtr<nsIFile> f;

  EXPECT_TRUE(NS_SUCCEEDED(
      NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(f))));

  EXPECT_NS_SUCCEEDED(f->Append(aLeaf));

  bool exists;
  EXPECT_TRUE(NS_SUCCEEDED(f->Exists(&exists)) && exists);

  nsString ret;
  EXPECT_NS_SUCCEEDED(f->GetPath(ret));
  return ret;
}

struct BinaryOrgNameInfo {
  wchar_t* dllPath;
  nsString orgName;
  bool* hasNestedMicrosoftCertificate;
};

static unsigned int __stdcall GetBinaryOrgName(void* binaryOrgNameInfo) {
  BinaryOrgNameInfo* info =
      reinterpret_cast<BinaryOrgNameInfo*>(binaryOrgNameInfo);
  RefPtr<DllServices> dllSvc(DllServices::Get());
  auto signedBy(dllSvc->GetBinaryOrgName(
      info->dllPath, info->hasNestedMicrosoftCertificate,
      AuthenticodeFlags::SkipTrustVerification));
  info->orgName.Assign(nsString(signedBy.get()));
  return 0;
}

// See bug 1817026
TEST(TestDllServices, GetNestedMicrosoftCertificate)
{
  constexpr auto kLeafName = u"msvcp140.dll"_ns;
  nsString dllPath = GetFullDllPath(kLeafName);
  bool hasNestedMicrosoftCertificate;
  BinaryOrgNameInfo info;
  info.dllPath = dllPath.get();
  info.hasNestedMicrosoftCertificate = &hasNestedMicrosoftCertificate;

  // Have to call GetBinaryOrgNames() off the main thread
  nsAutoHandle threadW(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, GetBinaryOrgName, &info, 0, nullptr)));
  ::WaitForSingleObject(threadW, INFINITE);
  EXPECT_EQ(info.orgName,
            u"Microsoft Windows Software Compatibility Publisher"_ns);
  EXPECT_TRUE(*info.hasNestedMicrosoftCertificate);
}

TEST(TestDllServices, DoNotGetNestedMicrosoftCertificate)
{
  constexpr auto kLeafName = u"msvcp140.dll"_ns;
  nsString dllPath = GetFullDllPath(kLeafName);
  BinaryOrgNameInfo info;
  info.dllPath = dllPath.get();
  // Mostly just to make sure we don't crash
  info.hasNestedMicrosoftCertificate = nullptr;

  // Have to call GetMainBinaryOrgName() off the main thread
  nsAutoHandle threadW(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, GetBinaryOrgName, &info, 0, nullptr)));
  ::WaitForSingleObject(threadW, INFINITE);
  EXPECT_EQ(info.orgName,
            u"Microsoft Windows Software Compatibility Publisher"_ns);
}
