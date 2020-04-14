/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExternalHelperAppService_h__
#define nsExternalHelperAppService_h__

#include "mozilla/Logging.h"
#include "prtime.h"

#include "nsIExternalHelperAppService.h"
#include "nsIExternalProtocolService.h"
#include "nsIWebProgressListener2.h"
#include "nsIHelperAppLauncherDialog.h"

#include "nsIMIMEInfo.h"
#include "nsIMIMEService.h"
#include "nsINamed.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannel.h"
#include "nsIBackgroundFileSaver.h"

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsCOMArray.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

class nsExternalAppHandler;
class nsIMIMEInfo;
class nsITransfer;
class MaybeCloseWindowHelper;

/**
 * The helper app service. Responsible for handling content that Mozilla
 * itself can not handle
 * Note that this is an abstract class - we depend on appropriate subclassing
 * on a per-OS basis to implement some methods.
 */
class nsExternalHelperAppService : public nsIExternalHelperAppService,
                                   public nsPIExternalAppLauncher,
                                   public nsIExternalProtocolService,
                                   public nsIMIMEService,
                                   public nsIObserver,
                                   public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE
  NS_DECL_NSPIEXTERNALAPPLAUNCHER
  NS_DECL_NSIMIMESERVICE
  NS_DECL_NSIOBSERVER

  nsExternalHelperAppService();

  /**
   * Initializes internal state. Will be called automatically when
   * this service is first instantiated.
   */
  [[nodiscard]] nsresult Init();

  /**
   * nsIExternalProtocolService methods that we provide in this class. Other
   * methods should be implemented by per-OS subclasses.
   */
  NS_IMETHOD ExternalProtocolHandlerExists(const char* aProtocolScheme,
                                           bool* aHandlerExists) override;
  NS_IMETHOD IsExposedProtocol(const char* aProtocolScheme,
                               bool* aResult) override;
  NS_IMETHOD GetProtocolHandlerInfo(const nsACString& aScheme,
                                    nsIHandlerInfo** aHandlerInfo) override;
  NS_IMETHOD LoadURI(nsIURI* aURI,
                     nsIInterfaceRequestor* aWindowContext) override;
  NS_IMETHOD SetProtocolHandlerDefaults(nsIHandlerInfo* aHandlerInfo,
                                        bool aOSHandlerExists) override;

  /**
   * Given a string identifying an application, create an nsIFile representing
   * it. This function should look in $PATH for the application.
   * The base class implementation will first try to interpret platformAppPath
   * as an absolute path, and if that fails it will look for a file next to the
   * mozilla executable. Subclasses can override this method if they want a
   * different behaviour.
   * @param platformAppPath A platform specific path to an application that we
   *                        got out of the rdf data source. This can be a mac
   *                        file spec, a unix path or a windows path depending
   *                        on the platform
   * @param aFile           [out] An nsIFile representation of that platform
   *                        application path.
   */
  virtual nsresult GetFileTokenForPath(const char16_t* platformAppPath,
                                       nsIFile** aFile);

  NS_IMETHOD OSProtocolHandlerExists(const char* aScheme, bool* aExists) = 0;

  /**
   * Given an extension, get a MIME type string. If not overridden by
   * the OS-specific nsOSHelperAppService, will call into GetMIMEInfoFromOS
   * with an empty mimetype.
   * @return true if we successfully found a mimetype.
   */
  virtual bool GetMIMETypeFromOSForExtension(const nsACString& aExtension,
                                             nsACString& aMIMEType);

  static already_AddRefed<nsExternalHelperAppService> GetSingleton();

 protected:
  virtual ~nsExternalHelperAppService();

  /**
   * Searches the "extra" array of MIMEInfo objects for an object
   * with a specific type. If found, it will modify the passed-in
   * MIMEInfo. Otherwise, it will return an error and the MIMEInfo
   * will be untouched.
   * @param aContentType The type to search for.
   * @param aMIMEInfo    [inout] The mime info, if found
   */
  nsresult FillMIMEInfoForMimeTypeFromExtras(const nsACString& aContentType,
                                             nsIMIMEInfo* aMIMEInfo);
  /**
   * Searches the "extra" array of MIMEInfo objects for an object
   * with a specific extension.
   *
   * Does not change the MIME Type of the MIME Info.
   *
   * @see FillMIMEInfoForMimeTypeFromExtras
   */
  nsresult FillMIMEInfoForExtensionFromExtras(const nsACString& aExtension,
                                              nsIMIMEInfo* aMIMEInfo);

  /**
   * Searches the "extra" array for a MIME type, and gets its extension.
   * @param aExtension The extension to search for
   * @param aMIMEType [out] The found MIME type.
   * @return true if the extension was found, false otherwise.
   */
  bool GetTypeFromExtras(const nsACString& aExtension, nsACString& aMIMEType);

  /**
   * Logging Module. Usage: set MOZ_LOG=HelperAppService:level, where level
   * should be 2 for errors, 3 for debug messages from the cross- platform
   * nsExternalHelperAppService, and 4 for os-specific debug messages.
   */
  static mozilla::LazyLogModule mLog;

  // friend, so that it can access the nspr log module.
  friend class nsExternalAppHandler;

  /**
   * Helper function for ExpungeTemporaryFiles and ExpungeTemporaryPrivateFiles
   */
  static void ExpungeTemporaryFilesHelper(nsCOMArray<nsIFile>& fileList);
  /**
   * Helper function for DeleteTemporaryFileOnExit and
   * DeleteTemporaryPrivateFileWhenPossible
   */
  static nsresult DeleteTemporaryFileHelper(nsIFile* aTemporaryFile,
                                            nsCOMArray<nsIFile>& aFileList);
  /**
   * Functions related to the tempory file cleanup service provided by
   * nsExternalHelperAppService
   */
  void ExpungeTemporaryFiles();
  /**
   * Functions related to the tempory file cleanup service provided by
   * nsExternalHelperAppService (for the temporary files added during
   * the private browsing mode)
   */
  void ExpungeTemporaryPrivateFiles();

  /**
   * Array for the files that should be deleted
   */
  nsCOMArray<nsIFile> mTemporaryFilesList;
  /**
   * Array for the files that should be deleted (for the temporary files
   * added during the private browsing mode)
   */
  nsCOMArray<nsIFile> mTemporaryPrivateFilesList;

 private:
  nsresult DoContentContentProcessHelper(
      const nsACString& aMimeContentType, nsIRequest* aRequest,
      mozilla::dom::BrowsingContext* aContentContext, bool aForceSave,
      nsIInterfaceRequestor* aWindowContext,
      nsIStreamListener** aStreamListener);
};

/**
 * An external app handler is just a small little class that presents itself as
 * a nsIStreamListener. It saves the incoming data into a temp file. The handler
 * is bound to an application when it is created. When it receives an
 * OnStopRequest it launches the application using the temp file it has
 * stored the data into.  We create a handler every time we have to process
 * data using a helper app.
 */
class nsExternalAppHandler final : public nsIStreamListener,
                                   public nsIHelperAppLauncher,
                                   public nsIBackgroundFileSaverObserver,
                                   public nsINamed {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIHELPERAPPLAUNCHER
  NS_DECL_NSICANCELABLE
  NS_DECL_NSIBACKGROUNDFILESAVEROBSERVER
  NS_DECL_NSINAMED

  /**
   * @param aMIMEInfo       MIMEInfo object, representing the type of the
   *                        content that should be handled
   * @param aFileExtension  The extension we need to append to our temp file,
   *                        INCLUDING the ".". e.g. .mp3
   * @param aContentContext dom Window context, as passed to DoContent.
   * @param aWindowContext  Top level window context used in dialog parenting,
   *                        as passed to DoContent. This parameter may be null,
   *                        in which case dialogs will be parented to
   *                        aContentContext.
   * @param mExtProtSvc     nsExternalHelperAppService on creation
   * @param aFileName       The filename to use
   * @param aReason         A constant from nsIHelperAppLauncherDialog
   * indicating why the request is handled by a helper app.
   */
  nsExternalAppHandler(nsIMIMEInfo* aMIMEInfo, const nsACString& aFileExtension,
                       mozilla::dom::BrowsingContext* aBrowsingContext,
                       nsIInterfaceRequestor* aWindowContext,
                       nsExternalHelperAppService* aExtProtSvc,
                       const nsAString& aFilename, uint32_t aReason,
                       bool aForceSave);

  /**
   * Clean up after the request was diverted to the parent process.
   */
  void DidDivertRequest(nsIRequest* request);

  /**
   * Apply content conversions if needed.
   */
  void MaybeApplyDecodingForExtension(nsIRequest* request);

  void SetShouldCloseWindow() { mShouldCloseWindow = true; }

 protected:
  ~nsExternalAppHandler();

  nsCOMPtr<nsIFile> mTempFile;
  nsCOMPtr<nsIURI> mSourceUrl;
  nsString mTempFileExtension;
  nsString mTempLeafName;

  /**
   * The MIME Info for this load. Will never be null.
   */
  nsCOMPtr<nsIMIMEInfo> mMimeInfo;

  /**
   * The BrowsingContext associated with this request to handle content.
   */
  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;

  /**
   * If set, the parent window helper app dialogs and file pickers
   * should use in parenting. If null, we use mContentContext.
   */
  nsCOMPtr<nsIInterfaceRequestor> mWindowContext;

  /**
   * Used to close the window on a timer, to avoid any exceptions that are
   * thrown if we try to close the window before it's fully loaded.
   */
  RefPtr<MaybeCloseWindowHelper> mMaybeCloseWindowHelper;

  /**
   * The following field is set if we were processing an http channel that had
   * a content disposition header which specified the SUGGESTED file name we
   * should present to the user in the save to disk dialog.
   */
  nsString mSuggestedFileName;

  /**
   * If set, this handler should forcibly save the file to disk regardless of
   * MIME info settings or anything else, without ever popping up the
   * unknown content type handling dialog.
   */
  bool mForceSave;

  /**
   * The canceled flag is set if the user canceled the launching of this
   * application before we finished saving the data to a temp file.
   */
  bool mCanceled;

  /**
   * True if a stop request has been issued.
   */
  bool mStopRequestIssued;

  bool mIsFileChannel;

  /**
   * True if the ExternalHelperAppChild told us that we should close the window
   * if we handle the content as a download.
   */
  bool mShouldCloseWindow;

  /**
   * One of the REASON_ constants from nsIHelperAppLauncherDialog. Indicates the
   * reason the dialog was shown (unknown content type, server requested it,
   * etc).
   */
  uint32_t mReason;

  /**
   * Track the executable-ness of the temporary file.
   */
  bool mTempFileIsExecutable;

  PRTime mTimeDownloadStarted;
  int64_t mContentLength;
  int64_t mProgress; /**< Number of bytes received (for sending progress
                        notifications). */

  /**
   * When we are told to save the temp file to disk (in a more permament
   * location) before we are done writing the content to a temp file, then
   * we need to remember the final destination until we are ready to use it.
   */
  nsCOMPtr<nsIFile> mFinalFileDestination;

  uint32_t mBufferSize;

  /**
   * This object handles saving the data received from the network to a
   * temporary location first, and then move the file to its final location,
   * doing all the input/output on a background thread.
   */
  nsCOMPtr<nsIBackgroundFileSaver> mSaver;

  /**
   * Stores the SHA-256 hash associated with the file that we downloaded.
   */
  nsAutoCString mHash;
  /**
   * Stores the signature information of the downloaded file in an nsTArray of
   * nsTArray of Array of bytes. If the file is unsigned this will be
   * empty.
   */
  nsTArray<nsTArray<nsTArray<uint8_t>>> mSignatureInfo;
  /**
   * Stores the redirect information associated with the channel.
   */
  nsCOMPtr<nsIArray> mRedirects;
  /**
   * Get the dialog parent: the parent window that we can attach
   * a dialog to when prompting the user for a download.
   */
  already_AddRefed<nsIInterfaceRequestor> GetDialogParent();
  /**
   * Creates the temporary file for the download and an output stream for it.
   * Upon successful return, both mTempFile and mSaver will be valid.
   */
  nsresult SetUpTempFile(nsIChannel* aChannel);
  /**
   * When we download a helper app, we are going to retarget all load
   * notifications into our own docloader and load group instead of
   * using the window which initiated the load....RetargetLoadNotifications
   * contains that information...
   */
  void RetargetLoadNotifications(nsIRequest* request);
  /**
   * Once the user tells us how they want to dispose of the content
   * create an nsITransfer so they know what's going on. If this fails, the
   * caller MUST call Cancel.
   */
  nsresult CreateTransfer();

  /**
   * If we fail to create the necessary temporary file to initiate a transfer
   * we will report the failure by creating a failed nsITransfer.
   */
  nsresult CreateFailedTransfer(bool aIsPrivateBrowsing);

  /*
   * The following two functions are part of the split of SaveToDisk
   * to make it async, and works as following:
   *
   *    SaveToDisk    ------->   RequestSaveDestination
   *                                     .
   *                                     .
   *                                     v
   *    ContinueSave  <-------   SaveDestinationAvailable
   */

  /**
   * This is called by SaveToDisk to decide what's the final
   * file destination chosen by the user or by auto-download settings.
   */
  void RequestSaveDestination(const nsString& aDefaultFile,
                              const nsString& aDefaultFileExt);

  /**
   * When SaveToDisk is called, it possibly delegates to RequestSaveDestination
   * to decide the file destination. ContinueSave must then be called when
   * the final destination is finally known.
   * @param  aFile  The file that was chosen as the final destination.
   *                Must not be null.
   */
  nsresult ContinueSave(nsIFile* aFile);

  /**
   * Notify our nsITransfer object that we are done with the download.  This is
   * always called after the target file has been closed.
   *
   * @param aStatus
   *        NS_OK for success, or a failure code if the download failed.
   *        A partially downloaded file may still be available in this case.
   */
  void NotifyTransfer(nsresult aStatus);

  /**
   * Helper routine that searches a pref string for a given mime type
   */
  bool GetNeverAskFlagFromPref(const char* prefName, const char* aContentType);

  /**
   * Helper routine to ensure that mTempFileExtension only contains an extension
   * when it is different from mSuggestedFileName's extension.
   */
  void EnsureTempFileExtension(const nsString& aFileExt);

  typedef enum { kReadError, kWriteError, kLaunchError } ErrorType;
  /**
   * Utility function to send proper error notification to web progress listener
   */
  void SendStatusChange(ErrorType type, nsresult aStatus, nsIRequest* aRequest,
                        const nsString& path);

  /**
   * Set in nsHelperDlgApp.js. This is always null after the user has chosen an
   * action.
   */
  nsCOMPtr<nsIWebProgressListener2> mDialogProgressListener;
  /**
   * Set once the user has chosen an action. This is null after the download
   * has been canceled or completes.
   */
  nsCOMPtr<nsITransfer> mTransfer;

  nsCOMPtr<nsIHelperAppLauncherDialog> mDialog;

  /**

   * The request that's being loaded. Initialized in OnStartRequest.
   * Nulled out in OnStopRequest or once we know what we're doing
   * with the data, whichever happens later.
   */
  nsCOMPtr<nsIRequest> mRequest;

  RefPtr<nsExternalHelperAppService> mExtProtSvc;
};

#endif  // nsExternalHelperAppService_h__
