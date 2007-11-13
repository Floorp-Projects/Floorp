/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Download Manager Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  httpserv = new nsHttpServer();
  httpserv.registerDirectory("/", dirSvc.get("ProfD", Ci.nsILocalFile));
  httpserv.start(4444);

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

      if (Ci.nsIDownloadManager.DOWNLOAD_FINISHED == aDownload.state) {
        httpserv.stop();
        do_test_finished();
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onStatusChange: function(a, b, c, d, e) { },
    onLocationChange: function(a, b, c, d) { },
    onSecurityChange: function(a, b, c, d) { }
  };
  dm.addListener(listener);

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(observer, "dl-start", false);

  addDownload();
  do_test_pending();
}
