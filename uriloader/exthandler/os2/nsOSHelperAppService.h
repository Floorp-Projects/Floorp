/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsOSHelperAppService_h__
#define nsOSHelperAppService_h__

// The OS helper app service is a subclass of nsExternalHelperAppService and is implemented on each
// platform. It contains platform specific code for finding helper applications for a given mime type
// in addition to launching those applications.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMIMEInfoImpl.h"
#include "nsCOMPtr.h"

#define LOG(args) PR_LOG(mLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(mLog, PR_LOG_DEBUG)

class nsHashtable;
class nsILineInputStream;
class nsMIMEInfoOS2;

class nsOSHelperAppService : public nsExternalHelperAppService
{
public:
  nsOSHelperAppService();
  virtual ~nsOSHelperAppService();

  // method overrides for mime.types and mime.info look up steps
  NS_IMETHOD GetFromTypeAndExtension(const nsACString& aMIMEType,
                                     const nsACString& aFileExt,
                                     nsIMIMEInfo **_retval);
  already_AddRefed<nsIMIMEInfo> GetMIMEInfoFromOS(const nsACString& aMimeType,
                                                  const nsACString& aFileExt,
                                                  PRBool     *aFound);
  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                          PRBool *found,
                                          nsIHandlerInfo **_retval);

  // override nsIExternalProtocolService methods
  NS_IMETHODIMP GetApplicationDescription(const nsACString& aScheme, nsAString& _retval);

  nsresult OSProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists);

protected:
  already_AddRefed<nsMIMEInfoOS2> GetFromType(const nsCString& aMimeType);
  already_AddRefed<nsMIMEInfoOS2> GetFromExtension(const nsCString& aFileExt);

private:
  // Helper methods which have to access static members
  static nsresult UnescapeCommand(const nsAString& aEscapedCommand,
                                  const nsAString& aMajorType,
                                  const nsAString& aMinorType,
                                  nsHashtable& aTypeOptions,
                                  nsACString& aUnEscapedCommand);
  static nsresult GetFileLocation(const char* aPrefName,
                                  const char* aEnvVarName,
                                  PRUnichar** aFileLocation);
  static nsresult LookUpTypeAndDescription(const nsAString& aFileExtension,
                                           nsAString& aMajorType,
                                           nsAString& aMinorType,
                                           nsAString& aDescription);
  static nsresult CreateInputStream(const nsAString& aFilename,
                                    nsIFileInputStream ** aFileInputStream,
                                    nsILineInputStream ** aLineInputStream,
                                    nsACString& aBuffer,
                                    PRBool * aNetscapeFormat,
                                    PRBool * aMore);

  static nsresult GetTypeAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                                         const nsAString& aFileExtension,
                                                         nsAString& aMajorType,
                                                         nsAString& aMinorType,
                                                         nsAString& aDescription);

  static nsresult LookUpExtensionsAndDescription(const nsAString& aMajorType,
                                                 const nsAString& aMinorType,
                                                 nsAString& aFileExtensions,
                                                 nsAString& aDescription);

  static nsresult GetExtensionsAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                                               const nsAString& aMajorType,
                                                               const nsAString& aMinorType,
                                                               nsAString& aFileExtensions,
                                                               nsAString& aDescription);

  static nsresult ParseNetscapeMIMETypesEntry(const nsAString& aEntry,
                                              nsAString::const_iterator& aMajorTypeStart,
                                              nsAString::const_iterator& aMajorTypeEnd,
                                              nsAString::const_iterator& aMinorTypeStart,
                                              nsAString::const_iterator& aMinorTypeEnd,
                                              nsAString& aExtensions,
                                              nsAString::const_iterator& aDescriptionStart,
                                              nsAString::const_iterator& aDescriptionEnd);

  static nsresult ParseNormalMIMETypesEntry(const nsAString& aEntry,
                                            nsAString::const_iterator& aMajorTypeStart,
                                            nsAString::const_iterator& aMajorTypeEnd,
                                            nsAString::const_iterator& aMinorTypeStart,
                                            nsAString::const_iterator& aMinorTypeEnd,
                                            nsAString& aExtensions,
                                            nsAString::const_iterator& aDescriptionStart,
                                            nsAString::const_iterator& aDescriptionEnd);

  static nsresult LookUpHandlerAndDescription(const nsAString& aMajorType,
                                              const nsAString& aMinorType,
                                              nsHashtable& aTypeOptions,
                                              nsAString& aHandler,
                                              nsAString& aDescription,
                                              nsAString& aMozillaFlags);
  static nsresult GetHandlerAndDescriptionFromMailcapFile(const nsAString& aFilename,
                                                          const nsAString& aMajorType,
                                                          const nsAString& aMinorType,
                                                          nsHashtable& aTypeOptions,
                                                          nsAString& aHandler,
                                                          nsAString& aDescription,
                                                          nsAString& aMozillaFlags);
};

#define MAXINIPARAMLENGTH 1024 // max length of OS/2 INI key for application parameters

// helper function for access to OS2.INI
nsresult GetApplicationAndParametersFromINI(const nsACString& aProtocol,
                                            char* app, unsigned long appLength,
                                            char* param, unsigned long paramLength);

#endif // nsOSHelperAppService_h__
