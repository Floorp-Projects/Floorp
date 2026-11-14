// SPDX-License-Identifier: MPL-2.0
#include "FloorpPrivateProfileTokenService.h"
#include "PrivateProfileToken.h"
#include "mozilla/Preferences.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceUtils.h"
#include "nsError.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace floorp::profile_token {
namespace {
bool Allowed() {
  if (!NS_IsMainThread() || !XRE_IsParentProcess() ||
      !mozilla::Preferences::GetBool("nora.dev.os_server.profile_token_file", false)) return false;
  nsAutoCString mode;
  if (NS_FAILED(mozilla::Preferences::GetCString("nora.startup.mode", mode))) return false;
  return mode.EqualsLiteral("dev") || mode.EqualsLiteral("test");
}
}
NS_IMPL_ISUPPORTS(PrivateProfileTokenService, nsIFloorpPrivateProfileToken)
PrivateProfileTokenService::PrivateProfileTokenService()
    : mStorage(std::make_unique<PrivateProfileToken>()) {}
PrivateProfileTokenService::~PrivateProfileTokenService() = default;

NS_IMETHODIMP PrivateProfileTokenService::GetProtocolVersion(uint32_t* aVersion) {
  NS_ENSURE_ARG_POINTER(aVersion);
  *aVersion = 1;
  return NS_OK;
}
NS_IMETHODIMP PrivateProfileTokenService::Prepare(const nsAString& aExpectedProfileRoot) {
  // Policy checks precede directory lookup and all native filesystem operations.
  if (!Allowed()) return NS_ERROR_NOT_AVAILABLE;
  nsCOMPtr<nsIFile> directory;
  nsresult result = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(directory));
  if (NS_FAILED(result) || !directory) return NS_ERROR_NOT_AVAILABLE;
  nsAutoString path;
  result = directory->GetPath(path);
  if (NS_FAILED(result) || !path.Equals(aExpectedProfileRoot)) return NS_ERROR_NOT_AVAILABLE;
  std::wstring native;
  native.reserve(path.Length());
  for (uint32_t index = 0; index < path.Length(); ++index) native.push_back(static_cast<wchar_t>(path[index]));
  return mStorage->Prepare(native) ? NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP PrivateProfileTokenService::Write(const nsACString& aToken) {
  if (!Allowed()) return NS_ERROR_NOT_AVAILABLE;
  return mStorage->Write(std::string_view(aToken.BeginReading(), aToken.Length())) ? NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP PrivateProfileTokenService::Secure() {
  if (!Allowed()) return NS_ERROR_NOT_AVAILABLE;
  return mStorage->Secure() ? NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP PrivateProfileTokenService::Remove() {
  // Never re-resolve ProfD: opt-out or a directory-service change cannot redirect
  // cleanup to another profile or prevent withdrawing our captured advertisement.
  if (!NS_IsMainThread() || !XRE_IsParentProcess()) return NS_ERROR_NOT_AVAILABLE;
  if (mStorage->Remove()) return NS_OK;
  // A short-lived ordinary reader must close before Windows permits DELETE.
  // Keep the captured owner intact so the JS lifecycle can retry asynchronously.
  return mStorage->CleanupBlockedByReader() ? NS_ERROR_FILE_IS_LOCKED : NS_ERROR_FAILURE;
}
}
