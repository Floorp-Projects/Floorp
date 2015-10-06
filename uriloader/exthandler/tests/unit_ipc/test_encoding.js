
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/MockRegistrar.js");

do_get_profile();

var DownloadListener = {
  init: function () {
    let obs = Services.obs;
    obs.addObserver(this, "dl-done", true);
  },

  observe: function (subject, topic, data) {
    this.onFinished(subject, topic, data);
  },

  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsISupportsWeakReference) ||
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}
DownloadListener.init();

function HelperAppDlg() { }
HelperAppDlg.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),
  show: function (launcher, ctx, reason, usePrivateUI) {
    launcher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.saveToFile;
    launcher.launchWithApplication(null, false);
  }
}

// Override the download-manager-ui to prevent anyone from trying to open
// a window.
function DownloadMgrUI() { }
DownloadMgrUI.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI]),
  show: function (ir, aID, reason) { },

  visible: false,

  getAttention: function () { }
}

function AlertsSVC() { }
AlertsSVC.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService]),
  showAlertNotification: function (url, title, text, clickable, cookie, listener, name) { },
}

MockRegistrar.register("@mozilla.org/helperapplauncherdialog;1",
                       HelperAppDlg);
MockRegistrar.register("@mozilla.org/download-manager-ui;1",
                       DownloadMgrUI);
MockRegistrar.register("@mozilla.org/alerts-service;1",
                       AlertsSVC);

function initChildTestEnv()
{
  sendCommand('                                                                \
    const Cc = Components.classes;                                             \
    const Ci = Components.interfaces;                                          \
    const Cr = Components.results;                                             \
    const Cu = Components.utils;                                               \
    Cu.import("resource://gre/modules/Services.jsm");                          \
    function WindowContext() { }                                               \
                                                                               \
    WindowContext.prototype = {                                                \
      getInterface: function (iid) {                                           \
        if (iid.equals(Ci.nsIInterfaceRequestor) ||                            \
            iid.equals(Ci.nsIURIContentListener) ||                            \
            iid.equals(Ci.nsILoadGroup) ||                                     \
            iid.equals(Ci.nsIDocumentLoader) ||                                \
            iid.equals(Ci.nsIDOMWindow))                                       \
          return this;                                                         \
                                                                               \
        throw Cr.NS_ERROR_NO_INTERFACE;                                        \
      },                                                                       \
                                                                               \
      /* nsIURIContentListener */                                              \
      onStartURIOpen: function (uri) { },                                      \
      isPreferred: function (type, desiredtype) { return false; },             \
                                                                               \
      /* nsILoadGroup */                                                       \
      addRequest: function (request, context) { },                             \
      removeRequest: function (request, context, status) { }                   \
    };                                                                         \
                                                                               \
    var ioservice = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);\
    var uriloader = Cc["@mozilla.org/uriloader;1"].getService(Ci.nsIURILoader);\
  ');
}

function testFinisher(endFunc) {
  let ef = endFunc;
  return function (file) {
    ef(file);
    runNextTest();
  }
}

function runChildTestSet(set)
{
  DownloadListener.onFinished = testFinisher(set[2]);
  sendCommand('\
  let uri = ioservice.newURI("http://localhost:4444' + set[0] + '", null, null);                  \
  let channel = ioservice.newChannelFromURI2(uri,                                                 \
                                             null, /* aLoadingNode */                             \
                                             Services.scriptSecurityManager.getSystemPrincipal(), \
                                             null, /* aTriggeringPrincipal */                     \
                                             Ci.nsILoadInfo.SEC_NORMAL,                           \
                                             Ci.nsIContentPolicy.TYPE_OTHER);                     \
  uriloader.openURI(channel, Ci.nsIURILoader.IS_CONTENT_PREFERRED, new WindowContext());          \
  ');
}

var httpserver = null;
var currentTest = 0;
function runNextTest()
{
  if (currentTest == tests.length) {
    httpserver.stop(do_test_finished);
    return;
  }

  let set = tests[currentTest++];
  runChildTestSet(set);
}

const responseBody = [0x1f, 0x8b, 0x08, 0x00, 0x16, 0x5a, 0x8a, 0x48, 0x02,
		      0x03, 0x2b, 0x49, 0x2d, 0x2e, 0xe1, 0x02, 0x00, 0xc6,
		      0x35, 0xb9, 0x3b, 0x05, 0x00, 0x00, 0x00];

/*
 * First test:  a file with Content-Type application/x-gzip and Content-Encoding gzip
 * should not be decoded in a round-trip
 */
function testResponse1(metadata, response) {
  response.setHeader("Content-Type", "application/x-gzip", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Content-Disposition", "attachment", false);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);
  bos.writeByteArray(responseBody, responseBody.length);
}

function finishTest1(subject, topic, data) {
  let file = subject.QueryInterface(Ci.nsIDownload).targetFile;
  do_check_true(file.path.search("test1.gz") != 0);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
  fis.init(file, -1, -1, 0);
  let bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  let str = bis.readByteArray(bis.available());
  do_check_matches(str, responseBody);
}

/*
 * Second test:  a file with Content-Type text/html and Content-Encoding gzip
 * should not be decoded in a round-trip, if its filename ends in ".gz".
 * We specify a Content-disposition header to force it to be saved as a file.
 */
function testResponse2(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Content-Disposition", "attachment", false);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);
  bos.writeByteArray(responseBody, responseBody.length);
}

function finishTest2(subject, topic, data) {
  let file = subject.QueryInterface(Ci.nsIDownload).targetFile;
  do_check_true(file.path.search("test2.gz") != 0);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
  fis.init(file, -1, -1, 0);
  let bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  let str = bis.readByteArray(bis.available());
  do_check_matches(str, responseBody);
}

function testResponse3(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Content-Disposition", "attachment", false);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);
  bos.writeByteArray(responseBody, responseBody.length);
}

function finishTest3(subject, topic, data) {
  let file = subject.QueryInterface(Ci.nsIDownload).targetFile;
  do_check_true(file.path.search("test3.txt") != 0);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
  fis.init(file, -1, -1, 0);
  let bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  let str = bis.readByteArray(bis.available());
  let decodedBody = [ 116, 101, 115, 116, 10 ]; // 't','e','s','t','\n'
  do_check_matches(str, decodedBody);
}

var tests = [
  [ "/test1.gz", testResponse1, finishTest1 ],
  [ "/test2.gz", testResponse2, finishTest2 ],
  [ "/test3.txt", testResponse3, finishTest3 ],
];

function run_test() {
//  do_load_child_test_harness();
  httpserver = new HttpServer();
  httpserver.start(4444);
  do_test_pending();

  initChildTestEnv();

  for each (set in tests)
    httpserver.registerPathHandler(set[0], set[1]);

  runNextTest();
}
