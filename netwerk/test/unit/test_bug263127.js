Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var server;
const BUGID = "263127";

var listener = {
  QueryInterface: function(iid) {
    if (!iid.equals(nsIDownloadObserver) &&
        !iid.equals(nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },

  onDownloadComplete: function(downloader, request, ctxt, status, file) {
    do_test_pending();
    server.stop(do_test_finished);

    if (!file)
      do_throw("Download failed");

    try {
      file.remove(false);
    }
    catch (e) {
      do_throw(e);
    }

    do_check_false(file.exists());

    do_test_finished();
  }
}

function run_test() {
  // start server
  server = new HttpServer();
  server.start(-1);

  // Initialize downloader
  var channel = NetUtil.newChannel({
    uri: "http://localhost:" + server.identity.primaryPort + "/",
    loadUsingSystemPrincipal: true
  });
  var targetFile = Cc["@mozilla.org/file/directory_service;1"]
                     .getService(Ci.nsIProperties)
                     .get("TmpD", Ci.nsIFile);
  targetFile.append("bug" + BUGID + ".test");
  if (targetFile.exists())
    targetFile.remove(false);

  var downloader = Cc["@mozilla.org/network/downloader;1"]
                     .createInstance(Ci.nsIDownloader);
  downloader.init(listener, targetFile);

  // Start download
  channel.asyncOpen2(downloader);

  do_test_pending();
}
