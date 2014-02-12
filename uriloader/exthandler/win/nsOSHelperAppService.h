/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSHelperAppService_h__
#define nsOSHelperAppService_h__

// The OS helper app service is a subclass of nsExternalHelperAppService and is implemented on each
// platform. It contains platform specific code for finding helper applications for a given mime type
// in addition to launching those applications.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMIMEInfoImpl.h"
#include "nsCOMPtr.h"
#include <windows.h>

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#include <shlobj.h>

class nsMIMEInfoWin;

class nsOSHelperAppService : public nsExternalHelperAppService
{
public:
  nsOSHelperAppService();
  virtual ~nsOSHelperAppService();

  // override nsIExternalProtocolService methods
  nsresult OSProtocolHandlerExists(const char * aProtocolScheme, bool * aHandlerExists);
  nsresult LoadUriInternal(nsIURI * aURL);
  NS_IMETHOD GetApplicationDescription(const nsACString& aScheme, nsAString& _retval);

  // method overrides for windows registry look up steps....
  already_AddRefed<nsIMIMEInfo> GetMIMEInfoFromOS(const nsACString& aMIMEType, const nsACString& aFileExt, bool *aFound);
  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme, 
                                          bool *found,
                                          nsIHandlerInfo **_retval);

  /** Get the string value of a registry value and store it in result.
   * @return true on success, false on failure
   */
  static bool GetValueString(HKEY hKey, const char16_t* pValueName, nsAString& result);

  // Removes registry command handler parameters, quotes, and expands environment strings.
  static bool CleanupCmdHandlerPath(nsAString& aCommandHandler);

protected:
  nsresult GetDefaultAppInfo(const nsAString& aTypeName, nsAString& aDefaultDescription, nsIFile** aDefaultApplication);
  // Lookup a mime info by extension, using an optional type hint
  already_AddRefed<nsMIMEInfoWin> GetByExtension(const nsAFlatString& aFileExt, const char *aTypeHint = nullptr);
  nsresult FindOSMimeInfoForType(const char * aMimeContentType, nsIURI * aURI, char ** aFileExtension, nsIMIMEInfo ** aMIMEInfo);

  static nsresult GetMIMEInfoFromRegistry(const nsAFlatString& fileType, nsIMIMEInfo *pInfo);
  /// Looks up the type for the extension aExt and compares it to aType
  static bool typeFromExtEquals(const char16_t* aExt, const char *aType);

private:
  IApplicationAssociationRegistration* mAppAssoc;
};

#endif // nsOSHelperAppService_h__
