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
 *   Gavin Sharp <gavin@gavinsharp.com> (Original Author)
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

// This tests the "add to recent documents" functionality of the DM

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

const resultFileName = "test" + Date.now() + ".doc";

function checkResult() {
  do_check_true(checkRecentDocsFor(resultFileName));

  // delete the saved file
  var resultFile = dirSvc.get("ProfD", Ci.nsIFile);
  resultFile.append(resultFileName);
  resultFile.remove(false);

  do_test_finished();
}

function checkRecentDocsFor(aFileName) {
  var recentDocsKey = Cc["@mozilla.org/windows-registry-key;1"].
                        createInstance(Ci.nsIWindowsRegKey);
  var recentDocsPath =
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs";
  recentDocsKey.open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                     recentDocsPath,
                     Ci.nsIWindowsRegKey.ACCESS_READ);
  var count = recentDocsKey.valueCount;
  for (var i = 0; i < count; ++i) {
    var valueName = recentDocsKey.getValueName(i);
    var binValue = recentDocsKey.readBinaryValue(valueName);

    // "fields" in the data are separated by double nulls, use only the first
    var fileName = binValue.split("\0\0")[0];

    // Remove any embeded single nulls
    fileName = fileName.replace(/\x00/g, "");

    if (aFileName == fileName)
      return true;
  }
  return false;
}

var httpserv = null;
function run_test()
{
  // This test functionality only implemented on Windows.
  // Is there a better way of doing this?
  var httpPH = Cc["@mozilla.org/network/protocol;1?name=http"].
               getService(Ci.nsIHttpProtocolHandler);
  if (httpPH.platform != "Windows")
    return;

  // Don't finish until the download is finished
  do_test_pending();

  httpserv = new nsHttpServer();
  httpserv.registerDirectory("/", dirSvc.get("ProfD", Ci.nsILocalFile));
  httpserv.start(4444);

  var listener = {
    onDownloadStateChange: function test_401430_odsc(aState, aDownload) {
      if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
        // Need to run this from a timeout, because the SHAddToRecentDocs call
        // doesn't update the registry immediately.
        do_timeout(1000, "checkResult();");
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };

  dm.addListener(listener);
  dm.addListener(getDownloadListener());

  var dl = addDownload({resultFileName: resultFileName});

  cleanup();
}
