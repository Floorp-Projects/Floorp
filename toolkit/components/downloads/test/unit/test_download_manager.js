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

do_import_script("netwerk/test/httpserver/httpd.js");

function createURI(aObj)
{
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                        ios.newURI(aObj, null, null);
}

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);
var profileDir = null;
try {
  profileDir = dirSvc.get("ProfD", Ci.nsIFile);
} catch (e) { }
if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfD") {
        return dirSvc.get("CurProcD", Ci.nsILocalFile);
      } else if (prop = "DLoads") {
        var file = dirSvc.get("CurProcD", Ci.nsILocalFile);
        file.append("downloads.rdf");
        return file;
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}

function cleanup()
{
  // removing rdf
  var rdfFile = dirSvc.get("DLoads", Ci.nsIFile);
  if (rdfFile.exists()) rdfFile.remove(true);
  
  // removing database
  var dbFile = dirSvc.get("ProfD", Ci.nsIFile);
  dbFile.append("downloads.sqlite");
  if (dbFile.exists()) dbFile.remove(true);

  // removing downloaded file
  var destFile = dirSvc.get("ProfD", Ci.nsIFile);
  destFile.append("download.result");
  if (destFile.exists()) destFile.remove(true);
}

//cleanup();

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function test_get_download_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_get_download_empty_queue");
  try {
    dm.getDownload(0);
    do_throw("Hey!  We expect to get an excpetion with this!");
  } catch(e) {
    do_check_eq(Components.lastResult, Cr.NS_ERROR_FAILURE);
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
    do_check_eq(Cr.NS_ERROR_FAILED, e.error);
  }
}

function test_resumeDownload_empty_queue()
{
  print("*** DOWNLOAD MANAGER TEST - test_resumeDownload_empty_queue");
  try {
    dm.resumeDownload(0);
    do_throw("This should not be reached");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILED, e.error);
  }
}

function addDownload()
{
  print("*** DOWNLOAD MANAGER TEST - Adding a download");
  const nsIWBP = Ci.nsIWebBrowserPersist;
  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                .createInstance(Ci.nsIWebBrowserPersist);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  var destFile = dirSvc.get("ProfD", Ci.nsIFile);
  destFile.append("download.result");
  var srcFile = dirSvc.get("ProfD", Ci.nsIFile);
  srcFile.append("LICENSE");

  var dl = dm.addDownload(nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI("http://localhost:4444/LICENSE"),
                          createURI(destFile), null, null, null,
                          Math.round(Date.now() * 1000), null, persist);

  // This will throw if it isn't found, and that would mean test failure, so no
  // try catch block
  var test = dm.getDownload(dl.id);

  // it is part of the active downloads now, even if it hasn't started.
  gDownloadCount++;

  persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

  print("*** DOWNLOAD MANAGER TEST - Adding a download worked");
  return dl;
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

var tests = [test_get_download_empty_queue, test_connection,
             test_count_empty_queue, test_canCleanUp_empty_queue,
             test_pauseDownload_empty_queue, test_resumeDownload_empty_queue,
             test_addDownload_normal, test_addDownload_cancel];

var gDownloadCount = 0;
var httpserv = null;
function run_test()
{
  print("*** DOWNLOAD MANAGER TEST - starting tests");
/*
  httpserv = new nsHttpServer();
  httpserv.registerDirectory("/", dirSvc.get("ProfD", Ci.nsILocalFile));
  httpserv.start(4444);
  print("*** DOWNLOAD MANAGER TEST - server started");
*/

  print("Try creating listener...")
  // our download listener
  var listener = {
    onDownloadStateChange: function(aState, aDownload)
    {
      if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED)
        gDownloadCount--;

      if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_CANCELED ||
          aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FAILED) {
          gDownloadCount--;
      }
      
      do_check_eq(gDownloadCount, dm.activeDownloadCount);
    
      if (gDownloadCount == 0)
        httpserv.stop();
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onStatusChange: function(a, b, c, d, e) { },
    onLocationChange: function(a, b, c, d) { },
    onSecurityChange: function(a, b, c, d) { }
  };
  dm.listener = listener;

  print("Try creating observer...");
  var observer = {
    observe: function(aSubject, aTopic, aData) {
      var dl = aSubject.QueryInterface(Ci.nsIDownload);
      switch (aTopic) {
        case "dl-start":
          do_check_eq(nsIDownloadManager.DOWNLOAD_DOWNLOADING, dl.state);
          do_test_pending();
          break;
        case "dl-failed":
          do_check_eq(nsIDownloadManager.DOWNLOAD_FAILED, dl.state);
          do_check_true(dm.canCleanUp);
          do_test_finished();
          break;
        case "dl-cancel":
          do_check_eq(nsIDownloadManager.DOWNLOAD_CANCELED, dl.state);
          do_check_true(dm.canCleanUp);
          do_test_finished();
          break;
        case "dl-done":
          dm.removeDownload(dl.id);

          var stmt = dm.DBConnection.createStmt("SELECT COUNT(*) " +
                                                "FROM moz_downloads " +
                                                "WHERE id = ?1");
          stmt.bindInt32Parameter(0, dl.id);
          stmt.executeStep();

          do_check_eq(0, stmt.getInt32(0));
          stmt.reset();

          do_check_eq(nsIDownloadManager.DOWNLOAD_FINISHED, dl.state);
          do_check_true(dm.canCleanUp);
          do_test_finished();
          break;
      };
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"]
           .getService(Ci.nsIObserverService);
  os.addObserver(observer, "dl-start", false);
  os.addObserver(observer, "dl-failed", false);
  os.addObserver(observer, "dl-cancel", false);
  os.addObserver(observer, "dl-done", false);

  print("Made it through adding observers.");

//  for (var i = 0; i < tests.length; i++)
//    tests[i]();

  //cleanup();

/*
  try {
    var thread = Cc["@mozilla.org/thread-manager;1"]
                 .getService().currentThread;

    while (!httpserv.isStopped())
      thread.processNextEvent(true);

    // get rid of any pending requests
    while (thread.hasPendingEvents())
      thread.processNextEvent(true);
  } catch (e) {
    print(e);
  }
*/
}
