/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests Bug 593815 - specifically that downloaded files
// are cleaned up correctly when a download is cancelled

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
do_load_manifest("test_downloads.manifest");

let httpserver = null;
let currentTest = 0;

function WindowContext() { }
WindowContext.prototype = {
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIInterfaceRequestor]),
  getInterface: XPCOMUtils.generateQI([Ci.nsIURIContentListener,
                                       Ci.nsILoadGroup]),

  /* nsIURIContentListener */
  onStartURIOpen: function (uri) { },
  isPreferred: function (type, desiredtype) { return false; },

  /* nsILoadGroup */
  addRequest: function (request, context) { },
  removeRequest: function (request, context, status) { }
};

let DownloadListener = {
  set : null,
  init: function () {
    Services.obs.addObserver(this, "dl-start", true);
    Services.obs.addObserver(this, "dl-done", true);
    Services.obs.addObserver(this, "dl-cancel", true);
    Services.obs.addObserver(this, "dl-fail", true);
  },

  // check that relevant cancel operations have been performed
  // currently this just means removing the target file
  onCancel: function(aSubject, aTopic, aData) {
    let dl = aSubject.QueryInterface(Ci.nsIDownload);
    do_check_false(dl.targetFile.exists());
    runNextTest();
  },

  observe: function (aSubject, aTopic, aData) {
    switch(aTopic) {
      case "dl-start" :
        // cancel, pause, or resume the download
        // depending on parameters in this.set
        let dl = aSubject.QueryInterface(Ci.nsIDownload);

        if (this.set.doPause) {
          downloadUtils.downloadManager.pauseDownload(dl.id);
        }

        if (this.set.doResume) {
          downloadUtils.downloadManager.resumeDownload(dl.id);
        }

        downloadUtils.downloadManager.cancelDownload(dl.id);
        break;
      case "dl-cancel" :
        this.onCancel(aSubject, aTopic, aData);
        break;
      case "dl-fail" :
        do_throw("Download failed");
        break;
      case "dl-done" :
        do_throw("Download finished");
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
}

// loads the uri given in tests[currentTest]
// finishes tests if there are no more test files listed
function runNextTest()
{
  if (currentTest == tests.length) {
    httpserver.stop(do_test_finished);
    return;
  }

  let set = DownloadListener.set = tests[currentTest];
  currentTest++;
  let uri = "http://localhost:" +
             httpserver.identity.primaryPort +
             set.serverPath;
  let channel = NetUtil.newChannel2(uri,
                                    null,
                                    null,
                                    null,      // aLoadingNode
                                    Services.scriptSecurityManager.getSystemPrincipal(),
                                    null,      // aTriggeringPrincipal
                                    Ci.nsILoadInfo.SEC_NORMAL,
                                    Ci.nsIContentPolicy.TYPE_OTHER);
  let uriloader = Cc["@mozilla.org/uriloader;1"].getService(Ci.nsIURILoader);
  uriloader.openURI(channel, Ci.nsIURILoader.IS_CONTENT_PREFERRED,
                    new WindowContext());
}

// sends the responses for the files. sends the same content twice if we resume
// the download
function getResponse(aSet) {
  return function(aMetadata, aResponse) {
    aResponse.setHeader("Content-Type", "text/plain", false);
    if (aMetadata.hasHeader("Range")) {
      var matches = aMetadata.getHeader("Range").match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
      aResponse.setStatusLine(aMetadata.httpVersion, 206, "Partial Content");
      aResponse.bodyOutputStream.write(aSet.data, aSet.data.length);
      return;
    }
    aResponse.setHeader("Accept-Ranges", "bytes", false);
    aResponse.setHeader("Content-Disposition", "attachment; filename=test.txt;", false);
    aResponse.bodyOutputStream.write(aSet.data, aSet.data.length);
  }
}

// files to be downloaded. For these tests, files will be cancelled either:
//   1.) during the download
//   2.) while they are paused
//   3.) after they have been resumed
let tests = [
  { serverPath: "/test1.html", data: "Test data 1" },
  { serverPath: "/test2.html", data: "Test data 2", doPause: true },
  { serverPath: "/test3.html", data: "Test data 3", doPause: true, doResume: true},
];

function run_test() {
  if (oldDownloadManagerDisabled()) {
    return;
  }

  // setup a download listener to run tests during and after the download
  DownloadListener.init();
  Services.prefs.setBoolPref("browser.download.manager.showWhenStarting", false);

  httpserver = new HttpServer();
  httpserver.start(-1);
  do_test_pending();

  // setup files to be download, each with the same suggested filename
  // from the server, but with different contents
  for(let i = 0; i < tests.length; i++) {
    let set = tests[i];
    httpserver.registerPathHandler(set.serverPath, getResponse(set));
  }

  runNextTest(); // start downloading the first file
}
