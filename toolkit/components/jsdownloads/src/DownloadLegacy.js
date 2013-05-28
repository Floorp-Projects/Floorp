/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");

////////////////////////////////////////////////////////////////////////////////
//// DownloadLegacyTransfer

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

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsIWebProgressListener2,
                                         Ci.nsITransfer]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIWebProgressListener

  onStateChange: function DLT_onStateChange(aWebProgress, aRequest, aStateFlags,
                                            aStatus)
  {
    // Detect when the last file has been received, or the download failed.
    if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
        (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {
      // Wait for the associated Download object to be available.
      this._deferDownload.promise.then(function DLT_OSC_onDownload(aDownload) {
        aDownload.saver.onTransferFinished(aRequest, aStatus);
      }).then(null, Cu.reportError);
    }
  },

  onProgressChange: function DLT_onProgressChange(aWebProgress, aRequest,
                                                  aCurSelfProgress,
                                                  aMaxSelfProgress,
                                                  aCurTotalProgress,
                                                  aMaxTotalProgress)
  {
    return onProgressChange64(aWebProgress, aRequest, aCurSelfProgress,
                              aMaxSelfProgress, aCurTotalProgress,
                              aMaxTotalProgress);
  },

  onLocationChange: function () { },

  onStatusChange: function DLT_onStatusChange(aWebProgress, aRequest, aStatus,
                                              aMessage)
  {
    // The status change may optionally be received in addition to the state
    // change, but if no network request actually started, it is possible that
    // we only receive a status change with an error status code.
    if (!Components.isSuccessCode(aStatus)) {
      // Wait for the associated Download object to be available.
      this._deferDownload.promise.then(function DLT_OSC_onDownload(aDownload) {
        aDownload.saver.onTransferFinished(aRequest, aStatus);
      }).then(null, Cu.reportError);
    }
  },

  onSecurityChange: function () { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIWebProgressListener2

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

  //////////////////////////////////////////////////////////////////////////////
  //// nsITransfer

  init: function DLT_init(aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime,
                          aTempFile, aCancelable, aIsPrivate)
  {
    // Create a new Download object associated to a DownloadLegacySaver, and
    // wait for it to be available.  This operation may cause the entire
    // download system to initialize before the object is created.
    Downloads.createDownload({
      source: { uri: aSource, isPrivate: aIsPrivate },
      target: { file: aTarget.QueryInterface(Ci.nsIFileURL).file },
      saver: { type: "legacy" },
    }).then(function DLT_I_onDownload(aDownload) {
      // Now that the saver is available, hook up the cancellation handler.
      aDownload.saver.deferCanceled.promise
                     .then(function () aCancelable.cancel(Cr.NS_ERROR_ABORT))
                     .then(null, Cu.reportError);

      // Start the download before allowing it to be controlled.
      aDownload.start();

      // Start processing all the other events received through nsITransfer.
      this._deferDownload.resolve(aDownload);

      // Add the download to the list, allowing it to be seen and canceled.
      let list;
      if (aIsPrivate) {
        list = Downloads.getPrivateDownloadList();
      } else {
        list = Downloads.getPublicDownloadList();
      }
      return list.then(function (aList) aList.add(aDownload));
    }.bind(this)).then(null, Cu.reportError);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Private methods and properties

  /**
   * This deferred object contains a promise that is resolved with the Download
   * object associated with this nsITransfer instance, when it is available.
   */
  _deferDownload: null,
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadLegacyTransfer]);
