/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSHelperAppService_h__
#define nsOSHelperAppService_h__

// The OS helper app service is a subclass of nsExternalHelperAppService and is
// implemented on each platform. It contains platform specific code for finding
// helper applications for a given mime type in addition to launching those
// applications. This is the Mac version.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"

class nsIMimeInfo;

class nsOSHelperAppService : public nsExternalHelperAppService {
 public:
  virtual ~nsOSHelperAppService();

  // override nsIExternalProtocolService methods
  NS_IMETHOD GetApplicationDescription(const nsACString& aScheme,
                                       nsAString& _retval) override;

  nsresult GetMIMEInfoFromOS(const nsACString& aMIMEType,
                             const nsACString& aFileExt, bool* aFound,
                             nsIMIMEInfo** aMIMEInfo) override;

  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString& aScheme,
                                          bool* found,
                                          nsIHandlerInfo** _retval) override;

  // GetFileTokenForPath must be implemented by each platform.
  // platformAppPath --> a platform specific path to an application that we got
  //                     out of the rdf data source. This can be a mac file
  //                     spec, a unix path or a windows path depending on the
  //                     platform
  // aFile --> an nsIFile representation of that platform application path.
  MOZ_MUST_USE nsresult GetFileTokenForPath(const char16_t* platformAppPath,
                                            nsIFile** aFile) override;

  MOZ_MUST_USE nsresult OSProtocolHandlerExists(const char* aScheme,
                                                bool* aHandlerExists) override;
};

#endif  // nsOSHelperAppService_h__
