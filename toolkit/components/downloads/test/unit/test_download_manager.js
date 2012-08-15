/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  httpserv = new HttpServer();
  httpserv.registerDirectory("/", do_get_cwd());
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
}
