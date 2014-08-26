/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DownloadView"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Downloads.jsm");

this.DownloadView = {
  init: function() {
    Downloads.getList(Downloads.ALL)
             .then(list => list.addView(this))
             .catch(Cu.reportError);
  },

  onDownloadAdded: function(aDownload) {
    let dmWindow = Services.wm.getMostRecentWindow("Download:Manager");
    if (dmWindow) {
      dmWindow.focus();
    } else {
      Services.ww.openWindow(null,
                             "chrome://webapprt/content/downloads/downloads.xul",
                             "Download:Manager",
                             "chrome,dialog=no,resizable",
                             null);
    }
  },
};

DownloadView.init();
