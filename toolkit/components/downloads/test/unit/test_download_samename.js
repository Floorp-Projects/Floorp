/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests Bug 593815 - specifically that after downloading two files
// with the same name, the download manager still points to the correct files

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
  set: null,
  prevFiles : [],

  init: function () {
    Services.obs.addObserver(this, "dl-start", true);
    Services.obs.addObserver(this, "dl-done", true);
  },

  observe: function (aSubject, aTopic, aData) {
    
    if (aTopic == "dl-start") {
      // pause the download if requested
      if (this.set.doPause) {
        let dl = aSubject.QueryInterface(Ci.nsIDownload);
        // Don't pause immediately, otherwise the external helper app handler
        // won't be able to assign a permanent file name.
        do_execute_soon(function() {
          downloadUtils.downloadManager.pauseDownload(dl.id);
          do_timeout(1000, function() {
            downloadUtils.downloadManager.resumeDownload(dl.id);
          });
        });
      }
    } else if (aTopic == "dl-done") {
      // check that no two files have the same filename in the download manager
      let file = aSubject.QueryInterface(Ci.nsIDownload).targetFile;
      for each (let prevFile in this.prevFiles) {
        do_check_neq(file.leafName, prevFile.leafName);
      }
      this.prevFiles.push(file);
    
      // get the contents of the file
      let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
      fis.init(file, -1, -1, 0);
      var cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(Ci.nsIConverterInputStream);
      cstream.init(fis, "UTF-8", 0, 0);
    
      let val = "";
      let (str = {}) {  
        let read = 0;
        do {
          read = cstream.readString(0xffffffff, str);
          val += str.value;  
        } while (read != 0);  
      }
      cstream.close();

      // check if the file contents match the expected ones
      if (this.set.doPause) {
        do_check_eq(val, this.set.data + this.set.data); // files that have been paused have the same data twice
      } else {
        do_check_eq(val, this.set.data);
      }
      runNextTest(); // download the next file
    }
  },

  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference])
}

/*
  Each test will download a file from the server.
*/
function runNextTest()
{
  if (currentTest == tests.length) {
    for each (var file in DownloadListener.prevFiles) {
      try {
        file.remove(false);
      } catch (ex) {
        try {
          do_report_unexpected_exception(ex, "while removing " + file.path);
        } catch (ex if ex == Components.results.NS_ERROR_ABORT) {
          /* swallow */
        }
      }
    }
    httpserver.stop(do_test_finished);
    return;
  }
  let set = DownloadListener.set = tests[currentTest];
  currentTest++;

  let channel = NetUtil.newChannel("http://localhost:" +
                                   httpserver.identity.primaryPort +
                                   set.serverURL);
  let uriloader = Cc["@mozilla.org/uriloader;1"].getService(Ci.nsIURILoader);
  uriloader.openURI(channel, true, new WindowContext());
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

// files to be downloaded. All files will have the same suggested filename, but
// should contain different data. doPause will cause the download to pause and resume
// itself
let tests = [
  { serverURL: "/test1.html", data: "Test data 1", doPause: false },
  { serverURL: "/test2.html", data: "Test data 2", doPause: false },
  { serverURL: "/test3.html", data: "Test data 3", doPause: true }
];

function run_test() {
  if (oldDownloadManagerDisabled()) {
    return;
  }

  // setup a download listener to run tests after each download finished
  DownloadListener.init();
  Services.prefs.setBoolPref("browser.download.manager.showWhenStarting", false);

  httpserver = new HttpServer();
  httpserver.start(-1);
  do_test_pending();

  // setup files to be download, each with the same suggested filename
  // from the server, but with different contents
  for(let i = 0; i < tests.length; i++) {
    let set = tests[i];
    httpserver.registerPathHandler(set.serverURL, getResponse(set));
  }

  runNextTest();  // start downloading the first file
}
