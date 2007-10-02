# -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# vim:set expandtab ts=2 sw=2 sts=2 cin
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Blake Ross <blakeross@telocity.com> (Original Author) 
#   Ben Goodger <ben@bengoodger.com> (v2.0) 
#   Edward Lee <edward.lee@engineering.uiuc.edu>
#   Shawn Wilsher <me@shawnwilsher.com> (v3.0)
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * DownloadProgressListener "class" is used to help update download items shown
 * in the Download Manager UI such as displaying amount transferred, transfer
 * rate, and time left for each download.
 *
 * This class implements the nsIDownloadProgressListener interface.
 */
function DownloadProgressListener() {}

DownloadProgressListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  onDownloadStateChange: function dlPL_onDownloadStateChange(aState, aDownload)
  {
    var dl = getDownload(aDownload.id);
    switch (aDownload.state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
        // We'll have at least one active download now
        gDownloadsActiveTitle.hidden = false;
      case Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING:
        // if dl is non-null, the download is already added to the UI, so we
        // just make sure it is where it is supposed to be; otherwise, create it
        if (!dl)
          dl = this._createDownloadItem(aDownload);
        gDownloadsView.insertBefore(dl, gDownloadsActiveTitle.nextSibling);
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED:
        downloadCompleted(aDownload);
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
        downloadCompleted(aDownload);
        autoRemoveAndClose(aDownload);
        break;
    }

    // autoRemoveAndClose could have already closed our window...
    try {
      dl.setAttribute("state", aDownload.state);
      updateStatus(dl);
      gDownloadViewController.onCommandUpdate();
    } catch (e) { }
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
                             aMaxSelfProgress, aCurTotalProgress,
                             aMaxTotalProgress, aDownload)
  {
    var download = getDownload(aDownload.id);
    if (!download) {
      // d'oh - why this happens is complicated, let's just add it in
      download = this._createDownloadItem(aDownload);
      gDownloadsView.insertBefore(download, gDownloadsActiveTitle.nextSibling);
    }

    // any activity means we should have active downloads!
    gDownloadsActiveTitle.hidden = false;

    // Update this download's progressmeter
    if (aDownload.percentComplete == -1) {
      download.setAttribute("progressmode", "undetermined");
    } else {
      download.setAttribute("progressmode", "normal");
      download.setAttribute("progress", aDownload.percentComplete);
    }

    // Dispatch ValueChange for a11y
    var event = document.createEvent("Events");
    event.initEvent("ValueChange", true, true);
    document.getAnonymousElementByAttribute(download, "anonid", "progressmeter")
            .dispatchEvent(event);

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

  onLocationChange: function(aWebProgress, aRequest, aLocation, aDownload)
  {
  },

  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage, aDownload)
  {
  },

  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload)
  {
  },

  //////////////////////////////////////////////////////////////////////////////
  //// DownloadProgressListener

  _createDownloadItem: function(aDownload)
  {
    let uri = Cc["@mozilla.org/network/util;1"].
              getService(Ci.nsIIOService).newFileURI(aDownload.targetFile);
    let referrer = aDownload.referrer;
    return createDownloadItem(aDownload.id,
                              uri.spec,
                              aDownload.displayName,
                              aDownload.source.spec,
                              aDownload.state,
                              aDownload.percentComplete,
                              Math.round(aDownload.startTime / 1000),
                              referrer ? referrer.spec : null,
                              aDownload.amountTransferred,
                              aDownload.size);
  }
};
