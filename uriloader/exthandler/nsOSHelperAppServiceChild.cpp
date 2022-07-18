/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <sys/stat.h>
#include "mozilla/Logging.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsOSHelperAppServiceChild.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"
#include "nsIHandlerService.h"
#include "nsMimeTypes.h"
#include "nsMIMEInfoImpl.h"
#include "nsMemory.h"
#include "nsCExternalHandlerService.h"
#include "nsCRT.h"
#include "nsEmbedCID.h"

#undef LOG
#define LOG(...)                                                     \
  MOZ_LOG(nsExternalHelperAppService::sLog, mozilla::LogLevel::Info, \
          (__VA_ARGS__))
#undef LOG_ERR
#define LOG_ERR(...)                                                  \
  MOZ_LOG(nsExternalHelperAppService::sLog, mozilla::LogLevel::Error, \
          (__VA_ARGS__))
#undef LOG_ENABLED
#define LOG_ENABLED() \
  MOZ_LOG_TEST(nsExternalHelperAppService::sLog, mozilla::LogLevel::Info)

nsresult nsOSHelperAppServiceChild::ExternalProtocolHandlerExists(
    const char* aProtocolScheme, bool* aHandlerExists) {
  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_ERR("nsOSHelperAppServiceChild error: no handler service");
    return rv;
  }

  nsAutoCString scheme(aProtocolScheme);
  *aHandlerExists = false;
  rv = handlerSvc->ExistsForProtocol(scheme, aHandlerExists);
  LOG("nsOSHelperAppServiceChild::ExternalProtocolHandlerExists(): "
      "protocol: %s, result: %" PRId32,
      aProtocolScheme, static_cast<uint32_t>(rv));
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

nsresult nsOSHelperAppServiceChild::OSProtocolHandlerExists(const char* aScheme,
                                                            bool* aExists) {
  // Use ExternalProtocolHandlerExists() which includes the
  // OS-level check and remotes the call to the parent process.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::GetApplicationDescription(const nsACString& aScheme,
                                                     nsAString& aRetVal) {
  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_ERR("nsOSHelperAppServiceChild error: no handler service");
    return rv;
  }

  rv = handlerSvc->GetApplicationDescription(aScheme, aRetVal);
  LOG("nsOSHelperAppServiceChild::GetApplicationDescription(): "
      "scheme: %s, result: %" PRId32 ", description: %s",
      PromiseFlatCString(aScheme).get(), static_cast<uint32_t>(rv),
      NS_ConvertUTF16toUTF8(aRetVal).get());
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                             const nsACString& aFileExt,
                                             bool* aFound,
                                             nsIMIMEInfo** aMIMEInfo) {
  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_ERR("nsOSHelperAppServiceChild error: no handler service");
    return rv;
  }

  rv = handlerSvc->GetMIMEInfoFromOS(aMIMEType, aFileExt, aFound, aMIMEInfo);
  LOG("nsOSHelperAppServiceChild::GetMIMEInfoFromOS(): "
      "MIME type: %s, extension: %s, result: %" PRId32,
      PromiseFlatCString(aMIMEType).get(), PromiseFlatCString(aFileExt).get(),
      static_cast<uint32_t>(rv));
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::GetProtocolHandlerInfoFromOS(
    const nsACString& aScheme, bool* aFound, nsIHandlerInfo** aRetVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::IsCurrentAppOSDefaultForProtocol(
    const nsACString& aScheme, bool* aRetVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsOSHelperAppServiceChild::GetFileTokenForPath(
    const char16_t* platformAppPath, nsIFile** aFile) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
