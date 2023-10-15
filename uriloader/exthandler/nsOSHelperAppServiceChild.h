/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSHelperAppServiceChild_h__
#define nsOSHelperAppServiceChild_h__

#include "nsExternalHelperAppService.h"

class nsIMIMEInfo;

/*
 * Provides a generic implementation of the nsExternalHelperAppService
 * platform-specific methods by remoting calls to the parent process.
 * Only provides implementations for the methods needed in unprivileged
 * child processes.
 */
class nsOSHelperAppServiceChild : public nsExternalHelperAppService {
 public:
  nsOSHelperAppServiceChild(){};
  virtual ~nsOSHelperAppServiceChild() = default;

  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString& aScheme,
                                          bool* found,
                                          nsIHandlerInfo** _retval) override;

  nsresult GetFileTokenForPath(const char16_t* platformAppPath,
                               nsIFile** aFile) override;

  NS_IMETHOD ExternalProtocolHandlerExists(const char* aProtocolScheme,
                                           bool* aHandlerExists) override;

  NS_IMETHOD OSProtocolHandlerExists(const char* aScheme,
                                     bool* aExists) override;

  NS_IMETHOD GetApplicationDescription(const nsACString& aScheme,
                                       nsAString& aRetVal) override;
  NS_IMETHOD IsCurrentAppOSDefaultForProtocol(const nsACString& aScheme,
                                              bool* _retval) override;

  NS_IMETHOD GetMIMEInfoFromOS(const nsACString& aMIMEType,
                               const nsACString& aFileExt, bool* aFound,
                               nsIMIMEInfo** aMIMEInfo) override;
};

#endif  // nsOSHelperAppServiceChild_h__
