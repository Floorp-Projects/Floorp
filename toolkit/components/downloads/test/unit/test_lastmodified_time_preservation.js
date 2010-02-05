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
 * Michal Sciubidlo <michal.sciubidlo@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrey Ivanov <andrey.v.ivanov@gmail.com>
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

/**
 * Test last modification time saving for downloaded manager (bug 178506)
 */

const nsIF = Ci.nsIFile;
const nsIWBP = Ci.nsIWebBrowserPersist;
const nsIWPL = Ci.nsIWebProgressListener;
const nsIDM = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDM);
const resultFileName = "test_178506" + Date.now() + ".txt";
const timeHeader = "Sun, 09 Sep 2001 01:46:40 GMT";
const timeValue = 1000000000 * 1000; //Sun, 09 Sep 2001 01:46:40 GMT in miliseconds

function run_test()
{
  // Disable this test on windows due to bug 377307
  var isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
  if (isWindows)
    return;

  // Start the http server with any data.
  var data = "test_178506";
  var httpserv = new nsHttpServer();
  httpserv.registerPathHandler("/test_178506", function(meta, resp) {
    var body = data;
    resp.setHeader("Content-Type", "text/html", false);
    // The only Last-Modified header does matter for test.
    resp.setHeader("Last-Modified", timeHeader, false);
    resp.bodyOutputStream.write(body, body.length);
  });
  httpserv.start(4444);

  do_test_pending();

  // Setting up test when file downloading is finished.
  var listener = {
    onDownloadStateChange: function test_178506(aState, aDownload) {
      if (aDownload.state == nsIDM.DOWNLOAD_FINISHED) {
        do_check_eq(destFile.lastModifiedTime, timeValue);
        httpserv.stop(do_test_finished);
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };
  dm.addListener(listener);

  // Start download.
  var destFile = dirSvc.get("ProfD", nsIF);
  destFile.append(resultFileName);
  if (destFile.exists())
    destFile.remove(false);

  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
                createInstance(nsIWBP);

  var dl = dm.addDownload(nsIDM.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI("http://localhost:4444/test_178506"),
                          createURI(destFile), null, null,
                          Math.round(Date.now() * 1000), null, persist);
  persist.progressListener = dl.QueryInterface(nsIWPL);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile);
}
