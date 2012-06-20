//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/Attributes.h"

class nsAnnotationService MOZ_FINAL : public nsIAnnotationService
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
