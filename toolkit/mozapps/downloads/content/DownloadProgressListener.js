// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DownloadProgressListener "class" is used to help update download items shown
 * in the Download Manager UI such as displaying amount transferred, transfer
 * rate, and time left for each download.
 *
 * This class implements the nsIDownloadProgressListener interface.
 */
function DownloadProgressListener() {}

DownloadProgressListener.prototype = {
  // ////////////////////////////////////////////////////////////////////////////
  // // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener]),

  // ////////////////////////////////////////////////////////////////////////////
  // // nsIDownloadProgressListener

  onDownloadStateChange: function dlPL_onDownloadStateChange(aState, aDownload)
  {
    // Update window title in-case we don't get all progress notifications
    onUpdateProgress();

    let dl;
    let state = aDownload.state;
    switch (state) {
      case nsIDM.DOWNLOAD_QUEUED:
        prependList(aDownload);
        break;

      case nsIDM.DOWNLOAD_BLOCKED_POLICY:
        prependList(aDownload);
        // Should fall through, this is a final state but DOWNLOAD_QUEUED
        // is skipped. See nsDownloadManager::AddDownload.
      case nsIDM.DOWNLOAD_FAILED:
      case nsIDM.DOWNLOAD_CANCELED:
      case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
      case nsIDM.DOWNLOAD_DIRTY:
      case nsIDM.DOWNLOAD_FINISHED:
        downloadCompleted(aDownload);
        if (state == nsIDM.DOWNLOAD_FINISHED)
          autoRemoveAndClose(aDownload);
        break;
      case nsIDM.DOWNLOAD_DOWNLOADING: {
        dl = getDownload(aDownload.id);

        // At this point, we know if we are an indeterminate download or not
        dl.setAttribute("progressmode", aDownload.percentComplete == -1 ?
                                        "undetermined" : "normal");

        // As well as knowing the referrer
        let referrer = aDownload.referrer;
        if (referrer)
          dl.setAttribute("referrer", referrer.spec);

        break;
      }
    }

    // autoRemoveAndClose could have already closed our window...
    try {
      if (!dl)
        dl = getDownload(aDownload.id);

      // Update to the new state
      dl.setAttribute("state", state);

      // Update ui text values after switching states
      updateTime(dl);
      updateStatus(dl);
      updateButtons(dl);
    } catch (e) { }
  },

  onProgressChange: function dlPL_onProgressChange(aWebProgress, aRequest,
                                                   aCurSelfProgress,
                                                   aMaxSelfProgress,
                                                   aCurTotalProgress,
                                                   aMaxTotalProgress, aDownload)
  {
    var download = getDownload(aDownload.id);

    // Update this download's progressmeter
    if (aDownload.percentComplete != -1) {
      download.setAttribute("progress", aDownload.percentComplete);

      // Dispatch ValueChange for a11y
      let event = document.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      document.getAnonymousElementByAttribute(download, "anonid", "progressmeter")
              .dispatchEvent(event);
    }

    // Update the progress so the status can be correctly updated
    download.setAttribute("currBytes", aDownload.amountTransferred);
    download.setAttribute("maxBytes", aDownload.size);

    // Update the rest of the UI (bytes transferred, bytes total, download rate,
    // time remaining).
    updateStatus(download, aDownload);

    // Update window title
    onUpdateProgress();
  },

  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload)
  {
  },

  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload)
  {
  }
};
