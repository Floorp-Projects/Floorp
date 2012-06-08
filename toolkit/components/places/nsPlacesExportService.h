/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPlacesExportService_h_
#define nsPlacesExportService_h_

#include "nsIPlacesImportExportService.h"

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsIFaviconService.h"
#include "nsIAnnotationService.h"
#include "mozIAsyncLivemarks.h"
#include "nsINavHistoryService.h"
#include "nsINavBookmarksService.h"
#include "nsIChannel.h"

class nsPlacesExportService : public nsIPlacesImportExportService
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPLACESIMPORTEXPORTSERVICE
    nsPlacesExportService();

  /**
   * Obtains the service's object.
   */
  static nsPlacesExportService* GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  private:
    static nsPlacesExportService* gExportService;
    virtual ~nsPlacesExportService();

  protected:
    nsCOMPtr<nsIFaviconService> mFaviconService;
    nsCOMPtr<nsIAnnotationService> mAnnotationService;
    nsCOMPtr<nsINavBookmarksService> mBookmarksService;
    nsCOMPtr<nsINavHistoryService> mHistoryService;
    nsCOMPtr<mozIAsyncLivemarks> mLivemarkService;

    nsresult WriteContainer(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerHeader(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteTitle(nsINavHistoryResultNode* aItem, nsIOutputStream* aOutput);
    nsresult WriteItem(nsINavHistoryResultNode* aItem, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteLivemark(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerContents(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteSeparator(nsINavHistoryResultNode* aItem, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteDescription(PRInt64 aId, PRInt32 aType, nsIOutputStream* aOutput);

    inline nsresult EnsureServiceState() {
      NS_ENSURE_STATE(mHistoryService);
      NS_ENSURE_STATE(mFaviconService);
      NS_ENSURE_STATE(mAnnotationService);
      NS_ENSURE_STATE(mBookmarksService);
      NS_ENSURE_STATE(mLivemarkService);
      return NS_OK;
    }
};

#endif // nsPlacesExportService_h_
