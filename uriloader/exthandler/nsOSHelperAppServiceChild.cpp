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
#include "nsMIMEInfoChild.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIHandlerService.h"
#include "nsMimeTypes.h"
#include "nsMIMEInfoImpl.h"
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsEmbedCID.h"

#undef LOG
#define LOG(args) \
  MOZ_LOG(nsExternalHelperAppService::mLog, mozilla::LogLevel::Info, args)
#undef LOG_ERR
#define LOG_ERR(args) \
  MOZ_LOG(nsExternalHelperAppService::mLog, mozilla::LogLevel::Error, args)
#undef LOG_ENABLED
#define LOG_ENABLED() \
  MOZ_LOG_TEST(nsExternalHelperAppService::mLog, mozilla::LogLevel::Info)

nsresult nsOSHelperAppServiceChild::ExternalProtocolHandlerExists(
    const char* aProtocolScheme, bool* aHandlerExists) {
  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_ERR(("nsOSHelperAppServiceChild error: no handler service"));
    return rv;
  }

  nsAutoCString scheme(aProtocolScheme);
  *aHandlerExists = false;
  rv = handlerSvc->ExistsForProtocol(scheme, aHandlerExists);
  LOG(
      ("nsOSHelperAppServiceChild::OSProtocolHandlerExists(): "
       "protocol: %s, result: %" PRId32,
       aProtocolScheme, static_cast<uint32_t>(rv)));
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
    LOG_ERR(("nsOSHelperAppServiceChild error: no handler service"));
    return rv;
  }

  rv = handlerSvc->GetApplicationDescription(aScheme, aRetVal);
  LOG(
      ("nsOSHelperAppServiceChild::GetApplicationDescription(): "
       "scheme: %s, result: %" PRId32 ", description: %s",
       PromiseFlatCString(aScheme).get(), static_cast<uint32_t>(rv),
       NS_ConvertUTF16toUTF8(aRetVal).get()));
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                             const nsACString& aFileExt,
                                             bool* aFound,
                                             nsIMIMEInfo** aMIMEInfo) {
  RefPtr<nsChildProcessMIMEInfo> mimeInfo =
      new nsChildProcessMIMEInfo(aMIMEType);

  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc) {
    nsresult rv =
        handlerSvc->GetMIMEInfoFromOS(mimeInfo, aMIMEType, aFileExt, aFound);
    LOG(
        ("nsOSHelperAppServiceChild::GetMIMEInfoFromOS(): "
         "MIME type: %s, extension: %s, result: %" PRId32,
         PromiseFlatCString(aMIMEType).get(),
         PromiseFlatCString(aFileExt).get(), static_cast<uint32_t>(rv)));
    mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    LOG_ERR(("nsOSHelperAppServiceChild error: no handler service"));
    *aFound = false;
  }

  mimeInfo.forget(aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsOSHelperAppServiceChild::GetProtocolHandlerInfoFromOS(
    const nsACString& aScheme, bool* aFound, nsIHandlerInfo** aRetVal) {
  MOZ_ASSERT(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv =
      OSProtocolHandlerExists(PromiseFlatCString(aScheme).get(), aFound);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsChildProcessMIMEInfo* handlerInfo =
      new nsChildProcessMIMEInfo(aScheme, nsMIMEInfoBase::eProtocolInfo);
  NS_ENSURE_TRUE(handlerInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aRetVal = handlerInfo);

  if (!*aFound) {
    // Code that calls this requires an object no matter whether the OS has
    // something for us, so we return the empty object.
    return NS_OK;
  }

  nsAutoString description;
  rv = GetApplicationDescription(aScheme, description);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "GetApplicationDescription failed");
  handlerInfo->SetDefaultDescription(description);
  return NS_OK;
}

nsresult nsOSHelperAppServiceChild::GetFileTokenForPath(
    const char16_t* platformAppPath, nsIFile** aFile) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
