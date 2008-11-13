//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Annotation Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsAnnotationService_h___
#define nsAnnotationService_h___

#include "nsIAnnotationService.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "nsServiceManagerUtils.h"

class nsAnnotationService : public nsIAnnotationService
{
public:
  nsAnnotationService();

  nsresult Init();

  static nsresult InitTables(mozIStorageConnection* aDBConn);

  /**
   * Returns a cached pointer to the annotation service for consumers in the
   * places directory.
   */
  static nsAnnotationService* GetAnnotationService()
  {
    if (! gAnnotationService) {
      // note that we actually have to set the service to a variable here
      // because the work in do_GetService actually happens during assignment >:(
      nsresult rv;
      nsCOMPtr<nsIAnnotationService> serv(do_GetService("@mozilla.org/browser/annotation-service;1", &rv));
      NS_ENSURE_SUCCESS(rv, nsnull);

      // our constructor should have set the static variable. If it didn't,
      // something is wrong.
      NS_ASSERTION(gAnnotationService, "Annotation service creation failed");
    }
    // the service manager will keep the pointer to our service around, so
    // this should always be valid even if nobody currently has a reference.
    return gAnnotationService;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIANNOTATIONSERVICE

private:
  ~nsAnnotationService();

protected:
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;

  nsCOMPtr<mozIStorageStatement> mDBSetAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBSetItemAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBGetAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBGetItemAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBGetAnnotationNames;
  nsCOMPtr<mozIStorageStatement> mDBGetItemAnnotationNames;
  nsCOMPtr<mozIStorageStatement> mDBGetAnnotationFromURI;
  nsCOMPtr<mozIStorageStatement> mDBGetAnnotationFromItemId;
  nsCOMPtr<mozIStorageStatement> mDBGetAnnotationNameID;
  nsCOMPtr<mozIStorageStatement> mDBAddAnnotationName;
  nsCOMPtr<mozIStorageStatement> mDBAddAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBAddItemAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBRemoveAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBRemoveItemAnnotation;
  nsCOMPtr<mozIStorageStatement> mDBGetItemsWithAnnotation;

  nsCOMArray<nsIAnnotationObserver> mObservers;

  static nsAnnotationService* gAnnotationService;

  static const int kAnnoIndex_ID;
  static const int kAnnoIndex_PageOrItem;
  static const int kAnnoIndex_Name;
  static const int kAnnoIndex_MimeType;
  static const int kAnnoIndex_Content;
  static const int kAnnoIndex_Flags;
  static const int kAnnoIndex_Expiration;
  static const int kAnnoIndex_Type;
  static const int kAnnoIndex_DateAdded;
  static const int kAnnoIndex_LastModified;

  nsresult HasAnnotationInternal(PRInt64 aFkId, PRBool aIsBookmarkId,
                                 const nsACString& aName, PRBool* hasAnnotation,
                                 PRInt64* annotationID);
  nsresult StartGetAnnotationFromURI(nsIURI* aURI,
                                     const nsACString& aName);
  nsresult StartGetAnnotationFromItemId(PRInt64 aItemId,
                                        const nsACString& aName);
  nsresult StartSetAnnotation(PRInt64 aFkId,
                              PRBool aIsItemAnnotation,
                              const nsACString& aName,
                              PRInt32 aFlags,
                              PRUint16 aExpiration,
                              PRUint16 aType,
                              mozIStorageStatement** aStatement);
  nsresult SetAnnotationStringInternal(PRInt64 aItemId,
                                       PRBool aIsItemAnnotation,
                                       const nsACString& aName,
                                       const nsAString& aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);
  nsresult SetAnnotationInt32Internal(PRInt64 aFkId,
                                      PRBool aIsItemAnnotation,
                                      const nsACString& aName,
                                      PRInt32 aValue,
                                      PRInt32 aFlags,
                                      PRUint16 aExpiration);
  nsresult SetAnnotationInt64Internal(PRInt64 aFkId,
                                      PRBool aIsItemAnnotation,
                                      const nsACString& aName,
                                      PRInt64 aValue,
                                      PRInt32 aFlags,
                                      PRUint16 aExpiration);
  nsresult SetAnnotationDoubleInternal(PRInt64 aFkId,
                                       PRBool aIsItemAnnotation,
                                       const nsACString& aName,
                                       double aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);
  nsresult SetAnnotationBinaryInternal(PRInt64 aFkId,
                                       PRBool aIsItemAnnotation,
                                       const nsACString& aName,
                                       const PRUint8 *aData,
                                       PRUint32 aDataLen,
                                       const nsACString& aMimeType,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);
  nsresult RemoveAnnotationInternal(PRInt64 aFkId,
                                    PRBool aIsItemAnnotation,
                                    const nsACString& aName);
  static nsresult GetPlaceIdForURI(nsIURI* aURI, PRInt64* _retval,
                                   PRBool aAutoCreate = PR_TRUE);

  PRBool InPrivateBrowsingMode() const;

  void CallSetForPageObservers(nsIURI* aURI, const nsACString& aName);
  void CallSetForItemObservers(PRInt64 aItemId, const nsACString& aName);

public:
  nsresult GetPagesWithAnnotationCOMArray(const nsACString& aName,
                                          nsCOMArray<nsIURI>* aResults);
  nsresult GetItemsWithAnnotationTArray(const nsACString& aName,
                                        nsTArray<PRInt64>* aResult);
  nsresult GetAnnotationNamesTArray(PRInt64 aFkId, nsTArray<nsCString>* aResult,
                                    PRBool aIsFkItemId);
};

#endif /* nsAnnotationService_h___ */
