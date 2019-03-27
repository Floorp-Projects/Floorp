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
// applications.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"

class nsILineInputStream;
class nsMIMEInfoBase;

class nsOSHelperAppService : public nsExternalHelperAppService {
 public:
  virtual ~nsOSHelperAppService();

  // method overrides for mime.types and mime.info look up steps
  NS_IMETHOD GetMIMEInfoFromOS(const nsACString& aMimeType,
                               const nsACString& aFileExt, bool* aFound,
                               nsIMIMEInfo** aMIMEInfo) override;
  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString& aScheme,
                                          bool* found,
                                          nsIHandlerInfo** _retval) override;

  // override nsIExternalProtocolService methods
  nsresult OSProtocolHandlerExists(const char* aProtocolScheme,
                                   bool* aHandlerExists) override;
  NS_IMETHOD GetApplicationDescription(const nsACString& aScheme,
                                       nsAString& _retval) override;

  // GetFileTokenForPath must be implemented by each platform.
  // platformAppPath --> a platform specific path to an application that we got
  //                     out of the rdf data source. This can be a mac file
  //                     spec, a unix path or a windows path depending on the
  //                     platform
  // aFile --> an nsIFile representation of that platform application path.
  virtual nsresult GetFileTokenForPath(const char16_t* platformAppPath,
                                       nsIFile** aFile) override;

 protected:
  already_AddRefed<nsMIMEInfoBase> GetFromType(const nsCString& aMimeType);
  already_AddRefed<nsMIMEInfoBase> GetFromExtension(const nsCString& aFileExt);

 private:
  // Helper methods which have to access static members
  static nsresult UnescapeCommand(const nsAString& aEscapedCommand,
                                  const nsAString& aMajorType,
                                  const nsAString& aMinorType,
                                  nsACString& aUnEscapedCommand);
  static nsresult GetFileLocation(const char* aPrefName,
                                  const char* aEnvVarName,
                                  nsAString& aFileLocation);
  static nsresult LookUpTypeAndDescription(const nsAString& aFileExtension,
                                           nsAString& aMajorType,
                                           nsAString& aMinorType,
                                           nsAString& aDescription,
                                           bool aUserData);
  static nsresult CreateInputStream(const nsAString& aFilename,
                                    nsIFileInputStream** aFileInputStream,
                                    nsILineInputStream** aLineInputStream,
                                    nsACString& aBuffer, bool* aNetscapeFormat,
                                    bool* aMore);

  static nsresult GetTypeAndDescriptionFromMimetypesFile(
      const nsAString& aFilename, const nsAString& aFileExtension,
      nsAString& aMajorType, nsAString& aMinorType, nsAString& aDescription);

  static nsresult LookUpExtensionsAndDescription(const nsAString& aMajorType,
                                                 const nsAString& aMinorType,
                                                 nsAString& aFileExtensions,
                                                 nsAString& aDescription);

  static nsresult GetExtensionsAndDescriptionFromMimetypesFile(
      const nsAString& aFilename, const nsAString& aMajorType,
      const nsAString& aMinorType, nsAString& aFileExtensions,
      nsAString& aDescription);

  static nsresult ParseNetscapeMIMETypesEntry(
      const nsAString& aEntry, nsAString::const_iterator& aMajorTypeStart,
      nsAString::const_iterator& aMajorTypeEnd,
      nsAString::const_iterator& aMinorTypeStart,
      nsAString::const_iterator& aMinorTypeEnd, nsAString& aExtensions,
      nsAString::const_iterator& aDescriptionStart,
      nsAString::const_iterator& aDescriptionEnd);

  static nsresult ParseNormalMIMETypesEntry(
      const nsAString& aEntry, nsAString::const_iterator& aMajorTypeStart,
      nsAString::const_iterator& aMajorTypeEnd,
      nsAString::const_iterator& aMinorTypeStart,
      nsAString::const_iterator& aMinorTypeEnd, nsAString& aExtensions,
      nsAString::const_iterator& aDescriptionStart,
      nsAString::const_iterator& aDescriptionEnd);

  static nsresult LookUpHandlerAndDescription(const nsAString& aMajorType,
                                              const nsAString& aMinorType,
                                              nsAString& aHandler,
                                              nsAString& aDescription,
                                              nsAString& aMozillaFlags);

  static nsresult DoLookUpHandlerAndDescription(const nsAString& aMajorType,
                                                const nsAString& aMinorType,
                                                nsAString& aHandler,
                                                nsAString& aDescription,
                                                nsAString& aMozillaFlags,
                                                bool aUserData);

  static nsresult GetHandlerAndDescriptionFromMailcapFile(
      const nsAString& aFilename, const nsAString& aMajorType,
      const nsAString& aMinorType, nsAString& aHandler, nsAString& aDescription,
      nsAString& aMozillaFlags);
};

#endif  // nsOSHelperAppService_h__
