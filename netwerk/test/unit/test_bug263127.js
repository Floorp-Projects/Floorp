"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var server;
const BUGID = "263127";

var listener = {
  QueryInterface: ChromeUtils.generateQI(["nsIDownloadObserver"]),

  onDownloadComplete(downloader, request, ctxt, status, file) {
    do_test_pending();
    server.stop(do_test_finished);

    if (!file) {
      do_throw("Download failed");
    }

    try {
      file.remove(false);
    } catch (e) {
      do_throw(e);
    }

    Assert.ok(!file.exists());

    do_test_finished();
  },
};

function run_test() {
  // start server
  server = new HttpServer();
  server.start(-1);

  // Initialize downloader
  var channel = NetUtil.newChannel({
    uri: "http://localhost:" + server.identity.primaryPort + "/",
    loadUsingSystemPrincipal: true,
  });
  var targetFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  targetFile.append("bug" + BUGID + ".test");
  if (targetFile.exists()) {
    targetFile.remove(false);
  }

  var downloader = Cc["@mozilla.org/network/downloader;1"].createInstance(
    Ci.nsIDownloader
  );
  downloader.init(listener, targetFile);

  // Start download
  channel.asyncOpen(downloader);

  do_test_pending();
}
