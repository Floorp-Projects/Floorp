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
#include "Database.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace places {

struct BookmarkData;

}  // namespace places
}  // namespace mozilla

class nsAnnotationService final : public nsIAnnotationService,
                                  public nsSupportsWeakReference {
  using BookmarkData = mozilla::places::BookmarkData;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIANNOTATIONSERVICE

  nsAnnotationService();

  /**
   * Obtains the service's object.
   */
  static already_AddRefed<nsAnnotationService> GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  /**
   * Returns a cached pointer to the annotation service for consumers in the
   * places directory.
   */
  static nsAnnotationService* GetAnnotationService() {
    if (!gAnnotationService) {
      nsCOMPtr<nsIAnnotationService> serv =
          do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nullptr);
      NS_ASSERTION(gAnnotationService,
                   "Should have static instance pointer now");
    }
    return gAnnotationService;
  }

 private:
  ~nsAnnotationService();

 protected:
  RefPtr<mozilla::places::Database> mDB;

  static nsAnnotationService* gAnnotationService;

  static const int kAnnoIndex_ID;
  static const int kAnnoIndex_PageOrItem;
  static const int kAnnoIndex_NameID;
  static const int kAnnoIndex_Content;
  static const int kAnnoIndex_Flags;
  static const int kAnnoIndex_Expiration;
  static const int kAnnoIndex_Type;
  static const int kAnnoIndex_DateAdded;
  static const int kAnnoIndex_LastModified;

  nsresult StartGetAnnotation(int64_t aItemId, const nsACString& aName,
                              nsCOMPtr<mozIStorageStatement>& aStatement);

  nsresult StartSetAnnotation(int64_t aItemId, BookmarkData* aBookmark,
                              const nsACString& aName, int32_t aFlags,
                              uint16_t aExpiration, uint16_t aType,
                              nsCOMPtr<mozIStorageStatement>& aStatement);

  nsresult SetAnnotationStringInternal(int64_t aItemId, BookmarkData* aBookmark,
                                       const nsACString& aName,
                                       const nsAString& aValue, int32_t aFlags,
                                       uint16_t aExpiration);
  nsresult SetAnnotationInt32Internal(int64_t aItemId, BookmarkData* aBookmark,
                                      const nsACString& aName, int32_t aValue,
                                      int32_t aFlags, uint16_t aExpiration);
  nsresult SetAnnotationInt64Internal(int64_t aItemId, BookmarkData* aBookmark,
                                      const nsACString& aName, int64_t aValue,
                                      int32_t aFlags, uint16_t aExpiration);
  nsresult SetAnnotationDoubleInternal(int64_t aItemId, BookmarkData* aBookmark,
                                       const nsACString& aName, double aValue,
                                       int32_t aFlags, uint16_t aExpiration);

  nsresult RemoveAnnotationInternal(int64_t aItemId, BookmarkData* aBookmark,
                                    const nsACString& aName);

  nsresult GetValueFromStatement(nsCOMPtr<mozIStorageStatement>& aStatement,
                                 nsIVariant** _retval);

 public:
  nsresult RemoveItemAnnotations(int64_t aItemId);
};

#endif /* nsAnnotationService_h___ */
