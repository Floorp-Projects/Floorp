/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExternalHelperAppService_h__
#define nsExternalHelperAppService_h__

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"
#include "prtime.h"

#include "nsIExternalHelperAppService.h"
#include "nsIExternalProtocolService.h"
#include "nsIWebProgressListener2.h"
#include "nsIHelperAppLauncherDialog.h"

#include "nsIMIMEInfo.h"
#include "nsIMIMEService.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIOutputStream.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannel.h"
#include "nsITimer.h"
#include "nsIBackgroundFileSaver.h"

#include "nsIHandlerService.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsCOMArray.h"
#include "nsWeakReference.h"
#include "nsIPrompt.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsExternalAppHandler;
class nsIMIMEInfo;
class nsITransfer;
class nsIDOMWindow;

/**
 * The helper app service. Responsible for handling content that Mozilla
 * itself can not handle
 */
class nsExternalHelperAppService
: public nsIExternalHelperAppService,
  public nsPIExternalAppLauncher,
  public nsIExternalProtocolService,
  public nsIMIMEService,
  public nsIObserver,
  public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE
  NS_DECL_NSPIEXTERNALAPPLAUNCHER
  NS_DECL_NSIEXTERNALPROTOCOLSERVICE
  NS_DECL_NSIMIMESERVICE
  NS_DECL_NSIOBSERVER

  nsExternalHelperAppService();
  virtual ~nsExternalHelperAppService();

  /**
   * Initializes internal state. Will be called automatically when
   * this service is first instantiated.
   */
  NS_HIDDEN_(nsresult) Init();
 
  /**
   * Given a mimetype and an extension, looks up a mime info from the OS.
   * The mime type is given preference. This function follows the same rules
   * as nsIMIMEService::GetFromTypeAndExtension.
   * This is supposed to be overridden by the platform-specific
   * nsOSHelperAppService!
   * @param aFileExt The file extension; may be empty. UTF-8 encoded.
   * @param [out] aFound
   *        Should be set to true if the os has a mapping, to
   *        false otherwise. Must not be null.
   * @return A MIMEInfo. This function must return a MIMEInfo object if it
   *         can allocate one.  The only justifiable reason for not
   *         returning one is an out-of-memory error.
   *         If null, the value of aFound is unspecified.
   */
  virtual already_AddRefed<nsIMIMEInfo> GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                          const nsACString& aFileExt,
                                                          bool       * aFound) = 0;

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
  virtual nsresult GetFileTokenForPath(const PRUnichar * platformAppPath,
                                       nsIFile ** aFile);

  virtual NS_HIDDEN_(nsresult) OSProtocolHandlerExists(const char *aScheme,
                                                       bool *aExists) = 0;

protected:
  /**
   * Searches the "extra" array of MIMEInfo objects for an object
   * with a specific type. If found, it will modify the passed-in
   * MIMEInfo. Otherwise, it will return an error and the MIMEInfo
   * will be untouched.
   * @param aContentType The type to search for.
   * @param aMIMEInfo    [inout] The mime info, if found
   */
  NS_HIDDEN_(nsresult) FillMIMEInfoForMimeTypeFromExtras(
    const nsACString& aContentType, nsIMIMEInfo * aMIMEInfo);
  /**
   * Searches the "extra" array of MIMEInfo objects for an object
   * with a specific extension.
   *
   * Does not change the MIME Type of the MIME Info.
   *
   * @see FillMIMEInfoForMimeTypeFromExtras
   */
  NS_HIDDEN_(nsresult) FillMIMEInfoForExtensionFromExtras(
    const nsACString& aExtension, nsIMIMEInfo * aMIMEInfo);

  /**
   * Searches the "extra" array for a MIME type, and gets its extension.
   * @param aExtension The extension to search for
   * @param aMIMEType [out] The found MIME type.
   * @return true if the extension was found, false otherwise.
   */
  NS_HIDDEN_(bool) GetTypeFromExtras(const nsACString& aExtension,
                                       nsACString& aMIMEType);

#ifdef PR_LOGGING
  /**
   * NSPR Logging Module. Usage: set NSPR_LOG_MODULES=HelperAppService:level,
   * where level should be 2 for errors, 3 for debug messages from the cross-
   * platform nsExternalHelperAppService, and 4 for os-specific debug messages.
   */
  static PRLogModuleInfo* mLog;

#endif
  // friend, so that it can access the nspr log module.
  friend class nsExternalAppHandler;

  /**
   * Helper function for ExpungeTemporaryFiles and ExpungeTemporaryPrivateFiles
   */
  static void ExpungeTemporaryFilesHelper(nsCOMArray<nsIFile> &fileList);
  /**
   * Helper function for DeleteTemporaryFileOnExit and DeleteTemporaryPrivateFileWhenPossible
   */
  static nsresult DeleteTemporaryFileHelper(nsIFile* aTemporaryFile,
                                            nsCOMArray<nsIFile> &aFileList);
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
};

/**
 * An external app handler is just a small little class that presents itself as
 * a nsIStreamListener. It saves the incoming data into a temp file. The handler
 * is bound to an application when it is created. When it receives an
 * OnStopRequest it launches the application using the temp file it has
 * stored the data into.  We create a handler every time we have to process
 * data using a helper app.
 */
class nsExternalAppHandler MOZ_FINAL : public nsIStreamListener,
                                       public nsIHelperAppLauncher,
                                       public nsITimerCallback,
                                       public nsIBackgroundFileSaverObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIHELPERAPPLAUNCHER
  NS_DECL_NSICANCELABLE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIBACKGROUNDFILESAVEROBSERVER

  /**
   * @param aMIMEInfo      MIMEInfo object, representing the type of the
   *                       content that should be handled
   * @param aFileExtension The extension we need to append to our temp file,
   *                       INCLUDING the ".". e.g. .mp3
   * @param aWindowContext Window context, as passed to DoContent
   * @param mExtProtSvc    nsExternalHelperAppService on creation
   * @param aFileName      The filename to use
   * @param aReason        A constant from nsIHelperAppLauncherDialog indicating
   *                       why the request is handled by a helper app.
   */
  nsExternalAppHandler(nsIMIMEInfo * aMIMEInfo, const nsCSubstring& aFileExtension,
                       nsIInterfaceRequestor * aWindowContext,
                       nsExternalHelperAppService * aExtProtSvc,
                       const nsAString& aFilename,
                       uint32_t aReason, bool aForceSave);

  ~nsExternalAppHandler();

protected:
  nsCOMPtr<nsIFile> mTempFile;
  nsCOMPtr<nsIURI> mSourceUrl;
  nsString mTempFileExtension;
  nsString mTempLeafName;

  /**
   * The MIME Info for this load. Will never be null.
   */
  nsCOMPtr<nsIMIMEInfo> mMimeInfo;
  nsCOMPtr<nsIInterfaceRequestor> mWindowContext;

  /**
   * Used to close the window on a timer, to avoid any exceptions that are
   * thrown if we try to close the window before it's fully loaded.
   */
  nsCOMPtr<nsIDOMWindow> mWindowToClose;
  nsCOMPtr<nsITimer> mTimer;

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
   * This is set based on whether the channel indicates that a new window
   * was opened specifically for this download.  If so, then we
   * close it.
   */
  bool mShouldCloseWindow;

  /**
   * True if a stop request has been issued.
   */
  bool mStopRequestIssued; 

  bool mIsFileChannel;

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
  int64_t mProgress; /**< Number of bytes received (for sending progress notifications). */

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
   * Creates the temporary file for the download and an output stream for it.
   * Upon successful return, both mTempFile and mSaver will be valid.
   */
  nsresult SetUpTempFile(nsIChannel * aChannel);
  /**
   * When we download a helper app, we are going to retarget all load
   * notifications into our own docloader and load group instead of
   * using the window which initiated the load....RetargetLoadNotifications
   * contains that information...
   */
  void RetargetLoadNotifications(nsIRequest *request);
  /**
   * Once the user tells us how they want to dispose of the content
   * create an nsITransfer so they know what's going on. If this fails, the
   * caller MUST call Cancel.
   */
  nsresult CreateTransfer();

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
  void RequestSaveDestination(const nsAFlatString &aDefaultFile,
                              const nsAFlatString &aDefaultFileExt);

  /**
   * When SaveToDisk is called, it possibly delegates to RequestSaveDestination
   * to decide the file destination. ContinueSave must then be called when
   * the final destination is finally known.
   * @param  aFile  The file that was chosen as the final destination.
   *                Must not be null.
   */
  nsresult ContinueSave(nsIFile* aFile);

  /**
   * After we're done prompting the user for any information, if the original
   * channel had a refresh url associated with it (which might point to a
   * "thank you for downloading" kind of page, then process that....It is safe
   * to invoke this method multiple times. We'll clear mOriginalChannel after
   * it's called and this ensures we won't call it again....
   */
  void ProcessAnyRefreshTags();

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
  bool GetNeverAskFlagFromPref(const char * prefName, const char * aContentType);

  /**
   * Helper routine to ensure mSuggestedFileName is "correct";
   * this ensures that mTempFileExtension only contains an extension when it
   * is different from mSuggestedFileName's extension.
   */
  void EnsureSuggestedFileName();

  typedef enum { kReadError, kWriteError, kLaunchError } ErrorType;
  /**
   * Utility function to send proper error notification to web progress listener
   */
  void SendStatusChange(ErrorType type, nsresult aStatus, nsIRequest *aRequest, const nsAFlatString &path);

  /**
   * Closes the window context if it does not have a refresh header
   * and it never displayed content before the external helper app
   * service was invoked.
   */
  nsresult MaybeCloseWindow();

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

  nsCOMPtr<nsIChannel> mOriginalChannel; /**< in the case of a redirect, this will be the pre-redirect channel. */
  nsCOMPtr<nsIHelperAppLauncherDialog> mDialog;

  /**
   * Keep request alive in case when helper non-modal dialog shown.
   * Thus in OnStopRequest the mRequest will not be set to null (it will be set to null further).
   */
  bool mKeepRequestAlive;

  /**
   * The request that's being loaded. Initialized in OnStartRequest.
   * Nulled out in OnStopRequest or once we know what we're doing
   * with the data, whichever happens later.
   */
  nsCOMPtr<nsIRequest> mRequest;

  nsRefPtr<nsExternalHelperAppService> mExtProtSvc;
};

#endif // nsExternalHelperAppService_h__
