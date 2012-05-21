/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
                                                  bool       *aFound);
  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                          bool *found,
                                          nsIHandlerInfo **_retval);

  // override nsIExternalProtocolService methods
  NS_IMETHODIMP GetApplicationDescription(const nsACString& aScheme, nsAString& _retval);

  nsresult OSProtocolHandlerExists(const char * aProtocolScheme, bool * aHandlerExists);

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
                                  nsAString& aFileLocation);
  static nsresult LookUpTypeAndDescription(const nsAString& aFileExtension,
                                           nsAString& aMajorType,
                                           nsAString& aMinorType,
                                           nsAString& aDescription);
  static nsresult CreateInputStream(const nsAString& aFilename,
                                    nsIFileInputStream ** aFileInputStream,
                                    nsILineInputStream ** aLineInputStream,
                                    nsACString& aBuffer,
                                    bool * aNetscapeFormat,
                                    bool * aMore);

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
