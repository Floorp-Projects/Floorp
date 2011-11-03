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
