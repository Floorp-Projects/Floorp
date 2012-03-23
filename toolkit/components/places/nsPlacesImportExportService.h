#ifndef nsPlacesImportExportService_h__
#define nsPlacesImportExportService_h__

#include "nsIPlacesImportExportService.h"

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsIOutputStream.h"
#include "nsIFaviconService.h"
#include "nsIAnnotationService.h"
#include "mozIAsyncLivemarks.h"
#include "nsINavHistoryService.h"
#include "nsINavBookmarksService.h"
#include "nsIChannel.h"

class nsPlacesImportExportService : public nsIPlacesImportExportService,
                                    public nsINavHistoryBatchCallback
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPLACESIMPORTEXPORTSERVICE
    NS_DECL_NSINAVHISTORYBATCHCALLBACK
    nsPlacesImportExportService();

  /**
   * Obtains the service's object.
   */
  static nsPlacesImportExportService* GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  private:
    static nsPlacesImportExportService* gImportExportService;
    virtual ~nsPlacesImportExportService();

  protected:
    nsCOMPtr<nsIFaviconService> mFaviconService;
    nsCOMPtr<nsIAnnotationService> mAnnotationService;
    nsCOMPtr<nsINavBookmarksService> mBookmarksService;
    nsCOMPtr<nsINavHistoryService> mHistoryService;
    nsCOMPtr<mozIAsyncLivemarks> mLivemarkService;

    nsCOMPtr<nsIChannel> mImportChannel;
    bool mIsImportDefaults;

    nsresult ImportHTMLFromFileInternal(nsILocalFile* aFile, bool aAllowRootChanges,
                                       PRInt64 aFolder, bool aIsImportDefaults);
    nsresult ImportHTMLFromURIInternal(nsIURI* aURI, bool aAllowRootChanges,
                                       PRInt64 aFolder, bool aIsImportDefaults);
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

#endif // nsPlacesImportExportService_h__
