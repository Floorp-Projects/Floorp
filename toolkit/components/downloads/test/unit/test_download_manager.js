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
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

// This file tests the download manager backend

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function test_get_download_empty_queue()
{
  try {
    dm.getDownload(0);
    do_throw("Hey!  We expect to get an excpetion with this!");
  } catch(e) {
    do_check_eq(Components.lastResult, Cr.NS_ERROR_NOT_AVAILABLE);
  }
}

function test_connection()
{
  print("*** DOWNLOAD MANAGER TEST - test_connection");
  var ds = dm.DBConnection;

  do_check_true(ds.connectionReady);

  do_check_true(ds.tableExists("moz_downloads"));
}

function test_count_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_count_empty_queue");
  do_check_eq(0, dm.activeDownloadCount);

  do_check_false(dm.activeDownloads.hasMoreElements());
}

function test_canCleanUp_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_canCleanUp_empty_queue");
  do_check_false(dm.canCleanUp);
}

function test_pauseDownload_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_pauseDownload_empty_queue");
  try {
    dm.pauseDownload(0);
    do_throw("This should not be reached");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
}

function test_resumeDownload_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_resumeDownload_empty_queue");
  try {
    dm.resumeDownload(0);
    do_throw("This should not be reached");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
}

function test_addDownload_normal()
{
  print("*** DOWNLOAD MANAGER TEST - Testing normal download adding");
  addDownload();
}

function test_addDownload_cancel()
{
  print("*** DOWNLOAD MANAGER TEST - Testing download cancel");
  var dl = addDownload();

  dm.cancelDownload(dl.id);

  do_check_eq(nsIDownloadManager.DOWNLOAD_CANCELED, dl.state);
}

// This test is actually ran by the observer 
function test_dm_getDownload(aDl)
{
  // this will get it from the database
  var dl = dm.getDownload(aDl.id);
  
  do_check_eq(aDl.displayName, dl.displayName);
}

var tests = [test_get_download_empty_queue, test_connection,
             test_count_empty_queue, test_canCleanUp_empty_queue,
             test_pauseDownload_empty_queue, test_resumeDownload_empty_queue,
             test_addDownload_normal, test_addDownload_cancel];

var httpserv = null;
function run_test()
{
  httpserv = new nsHttpServer();
  httpserv.registerDirectory("/", dirSvc.get("ProfD", Ci.nsILocalFile));
  httpserv.start(4444);

  // our download listener
  var listener = {
    // this listener checks to ensure activeDownloadCount is correct.
    onDownloadStateChange: function(aState, aDownload)
    {
      do_check_eq(gDownloadCount, dm.activeDownloadCount);
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };
  dm.addListener(listener);
  dm.addListener(getDownloadListener());

  var observer = {
    observe: function(aSubject, aTopic, aData) {
      var dl = aSubject.QueryInterface(Ci.nsIDownload);
      do_check_eq(nsIDownloadManager.DOWNLOAD_CANCELED, dl.state);
      do_check_true(dm.canCleanUp);
      test_dm_getDownload(dl);
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"]
           .getService(Ci.nsIObserverService);
  os.addObserver(observer, "dl-cancel", false);

  for (var i = 0; i < tests.length; i++)
    tests[i]();

  cleanup();
}
