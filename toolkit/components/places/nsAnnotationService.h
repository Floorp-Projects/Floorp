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
 *   Marco Bonardo <mak77@bonardo.net>
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
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"
#include "nsToolkitCompsCID.h"
#include "Database.h"
#include "nsString.h"

class nsAnnotationService : public nsIAnnotationService
                          , public nsIObserver
                          , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIANNOTATIONSERVICE
  NS_DECL_NSIOBSERVER

  nsAnnotationService();

  /**
   * Obtains the service's object.
   */
  static nsAnnotationService* GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  static nsAnnotationService* GetAnnotationServiceIfAvailable() {
    return gAnnotationService;
  }

  /**
   * Returns a cached pointer to the annotation service for consumers in the
   * places directory.
   */
  static nsAnnotationService* GetAnnotationService()
  {
    if (!gAnnotationService) {
      nsCOMPtr<nsIAnnotationService> serv =
        do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nsnull);
      NS_ASSERTION(gAnnotationService,
                   "Should have static instance pointer now");
    }
    return gAnnotationService;
  }

private:
  ~nsAnnotationService();

protected:
  nsRefPtr<mozilla::places::Database> mDB;

  nsCOMArray<nsIAnnotationObserver> mObservers;
  bool mHasSessionAnnotations;

  static nsAnnotationService* gAnnotationService;

  static const int kAnnoIndex_ID;
  static const int kAnnoIndex_PageOrItem;
  static const int kAnnoIndex_NameID;
  static const int kAnnoIndex_MimeType;
  static const int kAnnoIndex_Content;
  static const int kAnnoIndex_Flags;
  static const int kAnnoIndex_Expiration;
  static const int kAnnoIndex_Type;
  static const int kAnnoIndex_DateAdded;
  static const int kAnnoIndex_LastModified;

  nsresult HasAnnotationInternal(nsIURI* aURI,
                                 PRInt64 aItemId,
                                 const nsACString& aName,
                                 bool* _hasAnno);

  nsresult StartGetAnnotation(nsIURI* aURI,
                              PRInt64 aItemId,
                              const nsACString& aName,
                              nsCOMPtr<mozIStorageStatement>& aStatement);

  nsresult StartSetAnnotation(nsIURI* aURI,
                              PRInt64 aItemId,
                              const nsACString& aName,
                              PRInt32 aFlags,
                              PRUint16 aExpiration,
                              PRUint16 aType,
                              nsCOMPtr<mozIStorageStatement>& aStatement);

  nsresult SetAnnotationStringInternal(nsIURI* aURI,
                                       PRInt64 aItemId,
                                       const nsACString& aName,
                                       const nsAString& aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);
  nsresult SetAnnotationInt32Internal(nsIURI* aURI,
                                      PRInt64 aItemId,
                                      const nsACString& aName,
                                      PRInt32 aValue,
                                      PRInt32 aFlags,
                                      PRUint16 aExpiration);
  nsresult SetAnnotationInt64Internal(nsIURI* aURI,
                                      PRInt64 aItemId,
                                      const nsACString& aName,
                                      PRInt64 aValue,
                                      PRInt32 aFlags,
                                      PRUint16 aExpiration);
  nsresult SetAnnotationDoubleInternal(nsIURI* aURI,
                                       PRInt64 aItemId,
                                       const nsACString& aName,
                                       double aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);
  nsresult SetAnnotationBinaryInternal(nsIURI* aURI,
                                       PRInt64 aItemId,
                                       const nsACString& aName,
                                       const PRUint8* aData,
                                       PRUint32 aDataLen,
                                       const nsACString& aMimeType,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration);

  nsresult RemoveAnnotationInternal(nsIURI* aURI,
                                    PRInt64 aItemId,
                                    const nsACString& aName);

  bool InPrivateBrowsingMode() const;

public:
  nsresult GetPagesWithAnnotationCOMArray(const nsACString& aName,
                                          nsCOMArray<nsIURI>* _results);
  nsresult GetItemsWithAnnotationTArray(const nsACString& aName,
                                        nsTArray<PRInt64>* _result);
  nsresult GetAnnotationNamesTArray(nsIURI* aURI,
                                    PRInt64 aItemId,
                                    nsTArray<nsCString>* _result);
};

#endif /* nsAnnotationService_h___ */
