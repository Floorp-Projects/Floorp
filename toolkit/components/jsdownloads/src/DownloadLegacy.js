/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component implements the XPCOM interfaces required for integration with
 * the legacy download components.
 *
 * New code is expected to use the "Downloads.jsm" module directly, without
 * going through the interfaces implemented in this XPCOM component.  These
 * interfaces are only maintained for backwards compatibility with components
 * that still work synchronously on the main thread.
 */

"use strict";

// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

// DownloadLegacyTransfer

/**
 * nsITransfer implementation that provides a bridge to a Download object.
 *
 * Legacy downloads work differently than the JavaScript implementation.  In the
 * latter, the caller only provides the properties for the Download object and
 * the entire process is handled by the "start" method.  In the legacy
 * implementation, the caller must create a separate object to execute the
 * download, and then make the download visible to the user by hooking it up to
 * an nsITransfer instance.
 *
 * Since nsITransfer instances may be created before the download system is
 * initialized, and initialization as well as other operations are asynchronous,
 * this implementation is able to delay all progress and status notifications it
 * receives until the associated Download object is finally created.
 *
 * Conversely, the DownloadLegacySaver object can also receive execution and
 * cancellation requests asynchronously, before or after it is connected to
 * this nsITransfer instance.  For that reason, those requests are communicated
 * in a potentially deferred way, using promise objects.
 *
 * The component that executes the download implements nsICancelable to receive
 * cancellation requests, but after cancellation it cannot be reused again.
 *
 * Since the components that execute the download may be different and they
 * don't always give consistent results, this bridge takes care of enforcing the
 * expectations, for example by ensuring the target file exists when the
 * download is successful, even if the source has a size of zero bytes.
 */
function DownloadLegacyTransfer()
{
  this._deferDownload = Promise.defer();
}

DownloadLegacyTransfer.prototype = {
  classID: Components.ID("{1b4c85df-cbdd-4bb6-b04e-613caece083c}"),

  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsIWebProgressListener2,
                                         Ci.nsITransfer]),

  // nsIWebProgressListener

  onStateChange: function DLT_onStateChange(aWebProgress, aRequest, aStateFlags,
                                            aStatus)
  {
    if (!Components.isSuccessCode(aStatus)) {
      this._componentFailed = true;
    }

    if ((aStateFlags & Ci.nsIWebProgressListener.STATE_START) &&
        (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {

      let blockedByParentalControls = false;
      // If it is a failed download, aRequest.responseStatus doesn't exist.
      // (missing file on the server, network failure to download)
      try {
        // If the request's response has been blocked by Windows Parental Controls
        // with an HTTP 450 error code, we must cancel the request synchronously.
        blockedByParentalControls = aRequest instanceof Ci.nsIHttpChannel &&
                                      aRequest.responseStatus == 450;
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
          aRequest.cancel(Cr.NS_BINDING_ABORTED);
        }
      }

      if (blockedByParentalControls) {
        aRequest.cancel(Cr.NS_BINDING_ABORTED);
      }

      // The main request has just started.  Wait for the associated Download
      // object to be available before notifying.
      this._deferDownload.promise.then(download => {
        // If the request was blocked, now that we have the download object we
        // should set a flag that can be retrieved later when handling the
        // cancellation so that the proper error can be thrown.
        if (blockedByParentalControls) {
          download._blockedByParentalControls = true;
        }

        download.saver.onTransferStarted(
                         aRequest,
                         this._cancelable instanceof Ci.nsIHelperAppLauncher);

        // To handle asynchronous cancellation properly, we should hook up the
        // handler only after we have been notified that the main request
        // started.  We will wait until the main request stopped before
        // notifying that the download has been canceled.  Since the request has
        // not completed yet, deferCanceled is guaranteed to be set.
        return download.saver.deferCanceled.promise.then(() => {
          // Only cancel if the object executing the download is still running.
          if (this._cancelable && !this._componentFailed) {
            this._cancelable.cancel(Cr.NS_ERROR_ABORT);
            if (this._cancelable instanceof Ci.nsIWebBrowserPersist) {
              // This component will not send the STATE_STOP notification.
              download.saver.onTransferFinished(aRequest, Cr.NS_ERROR_ABORT);
              this._cancelable = null;
            }
          }
        });
      }).then(null, Cu.reportError);
    } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
        (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {
      // The last file has been received, or the download failed.  Wait for the
      // associated Download object to be available before notifying.
      this._deferDownload.promise.then(download => {
        // At this point, the hash has been set and we need to copy it to the
        // DownloadSaver.
        if (Components.isSuccessCode(aStatus)) {
          download.saver.setSha256Hash(this._sha256Hash);
          download.saver.setSignatureInfo(this._signatureInfo);
          download.saver.setRedirects(this._redirects);
        }
        download.saver.onTransferFinished(aRequest, aStatus);
      }).then(null, Cu.reportError);

      // Release the reference to the component executing the download.
      this._cancelable = null;
    }
  },

  onProgressChange: function DLT_onProgressChange(aWebProgress, aRequest,
                                                  aCurSelfProgress,
                                                  aMaxSelfProgress,
                                                  aCurTotalProgress,
                                                  aMaxTotalProgress)
  {
    this.onProgressChange64(aWebProgress, aRequest, aCurSelfProgress,
                            aMaxSelfProgress, aCurTotalProgress,
                            aMaxTotalProgress);
  },

  onLocationChange() { },

  onStatusChange: function DLT_onStatusChange(aWebProgress, aRequest, aStatus,
                                              aMessage)
  {
    // The status change may optionally be received in addition to the state
    // change, but if no network request actually started, it is possible that
    // we only receive a status change with an error status code.
    if (!Components.isSuccessCode(aStatus)) {
      this._componentFailed = true;

      // Wait for the associated Download object to be available.
      this._deferDownload.promise.then(function DLT_OSC_onDownload(aDownload) {
        aDownload.saver.onTransferFinished(aRequest, aStatus);
      }).then(null, Cu.reportError);
    }
  },

  onSecurityChange() { },

  // nsIWebProgressListener2

  onProgressChange64: function DLT_onProgressChange64(aWebProgress, aRequest,
                                                      aCurSelfProgress,
                                                      aMaxSelfProgress,
                                                      aCurTotalProgress,
                                                      aMaxTotalProgress)
  {
    // Wait for the associated Download object to be available.
    this._deferDownload.promise.then(function DLT_OPC64_onDownload(aDownload) {
      aDownload.saver.onProgressBytes(aCurTotalProgress, aMaxTotalProgress);
    }).then(null, Cu.reportError);
  },

  onRefreshAttempted: function DLT_onRefreshAttempted(aWebProgress, aRefreshURI,
                                                      aMillis, aSameURI)
  {
    // Indicate that refreshes and redirects are allowed by default.  However,
    // note that download components don't usually call this method at all.
    return true;
  },

  // nsITransfer

  init: function DLT_init(aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime,
                          aTempFile, aCancelable, aIsPrivate)
  {
    this._cancelable = aCancelable;

    let launchWhenSucceeded = false, contentType = null, launcherPath = null;

    if (aMIMEInfo instanceof Ci.nsIMIMEInfo) {
      launchWhenSucceeded =
                aMIMEInfo.preferredAction != Ci.nsIMIMEInfo.saveToDisk;
      contentType = aMIMEInfo.type;

      let appHandler = aMIMEInfo.preferredApplicationHandler;
      if (aMIMEInfo.preferredAction == Ci.nsIMIMEInfo.useHelperApp &&
          appHandler instanceof Ci.nsILocalHandlerApp) {
        launcherPath = appHandler.executable.path;
      }
    }

    // Create a new Download object associated to a DownloadLegacySaver, and
    // wait for it to be available.  This operation may cause the entire
    // download system to initialize before the object is created.
    Downloads.createDownload({
      source: { url: aSource.spec, isPrivate: aIsPrivate },
      target: { path: aTarget.QueryInterface(Ci.nsIFileURL).file.path,
                partFilePath: aTempFile && aTempFile.path },
      saver: "legacy",
      launchWhenSucceeded,
      contentType,
      launcherPath
    }).then(function DLT_I_onDownload(aDownload) {
      // Legacy components keep partial data when they use a ".part" file.
      if (aTempFile) {
        aDownload.tryToKeepPartialData = true;
      }

      // Start the download before allowing it to be controlled.  Ignore errors.
      aDownload.start().catch(() => {});

      // Start processing all the other events received through nsITransfer.
      this._deferDownload.resolve(aDownload);

      // Add the download to the list, allowing it to be seen and canceled.
      return Downloads.getList(Downloads.ALL).then(list => list.add(aDownload));
    }.bind(this)).then(null, Cu.reportError);
  },

  setSha256Hash(hash)
  {
    this._sha256Hash = hash;
  },

  setSignatureInfo(signatureInfo)
  {
    this._signatureInfo = signatureInfo;
  },

  setRedirects(redirects)
  {
    this._redirects = redirects;
  },

  // Private methods and properties

  /**
   * This deferred object contains a promise that is resolved with the Download
   * object associated with this nsITransfer instance, when it is available.
   */
  _deferDownload: null,

  /**
   * Reference to the component that is executing the download.  This component
   * allows cancellation through its nsICancelable interface.
   */
  _cancelable: null,

  /**
   * Indicates that the component that executes the download has notified a
   * failure condition.  In this case, we should never use the component methods
   * that cancel the download.
   */
  _componentFailed: false,

  /**
   * Save the SHA-256 hash in raw bytes of the downloaded file.
   */
  _sha256Hash: null,

  /**
   * Save the signature info in a serialized protobuf of the downloaded file.
   */
  _signatureInfo: null,
};

// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadLegacyTransfer]);
