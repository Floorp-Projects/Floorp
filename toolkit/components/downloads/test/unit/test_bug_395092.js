/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests Bug 395092 - specifically that dl-start event isn't
// dispatched for resumed downloads.

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

var observer = {
  mCount: 0,
  id: 0,
  observe: function observe(aSubject, aTopic, aData)
  {
    print("observering " + aTopic);
    if ("dl-start" == aTopic) {
      var dl = aSubject.QueryInterface(Ci.nsIDownload);
      this.id = dl.id;
      dm.pauseDownload(this.id);
      this.mCount++;
      do_check_eq(1, this.mCount);
    } else if ("timer-callback" == aTopic) {
      dm.resumeDownload(this.id);
    }
  }
};

var httpserv = null;
var timer = null;
function run_test()
{
  if (oldDownloadManagerDisabled()) {
    return;
  }

  httpserv = new HttpServer();
  httpserv.registerDirectory("/", do_get_cwd());
  httpserv.start(-1);

  // our download listener
  var listener = {
    onDownloadStateChange: function(aOldState, aDownload)
    {
      if (Ci.nsIDownloadManager.DOWNLOAD_PAUSED == aDownload.state) {
        // This is so hacky, but it let's the nsWebBrowserPersist catch up with
        // the script...
        timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.init(observer, 0, Ci.nsITimer.TYPE_ONE_SHOT);
      }

      if (Ci.nsIDownloadManager.DOWNLOAD_FINISHED == aDownload.state)
        do_test_finished();
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };
  dm.addListener(listener);
  dm.addListener(getDownloadListener());

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(observer, "dl-start", false);

  addDownload(httpserv);
  do_test_pending();
}
