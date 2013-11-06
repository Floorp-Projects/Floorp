/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_serialize_file: Add test engines
 *
 * Ensure that :
 * - File is created.
 * - File size is bigger than 0.
 * - lazySerializeTask updates the file.
 */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

add_test(function test_batchTask() {
  let observer = function(aSubject, aTopic, aData) {
    if (aTopic == "browser-search-engine-modified" && aData == "engine-loaded") {
      let engine1 = Services.search.getEngineByName("Test search engine");
      let engine2 = Services.search.getEngineByName("Sherlock test search engine");
      if (engine1 && engine2) {
        Services.obs.removeObserver(observer, aTopic);
        // Test that files are written correctly.
        let engineFile1 = engine1.wrappedJSObject._file;
        let engineFile2 = engine2.wrappedJSObject._file;
        do_check_true(engineFile1.exists());
        do_check_true(engineFile2.exists());
        do_check_neq(engineFile1.fileSize, 0);
        do_check_neq(engineFile2.fileSize, 0);
        run_next_test();
      }
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  Services.search.addEngine("http://localhost:4444/data/engine.xml",
                            Ci.nsISearchEngine.DATA_XML, null, false);
  Services.search.addEngine("http://localhost:4444/data/engine.src",
                            Ci.nsISearchEngine.DATA_TEXT,
                            "http://localhost:4444/data/ico-size-16x16-png.ico",
                            false);
});

add_test(function test_addParam() {
  let engine = Services.search.getEngineByName("Test search engine");
  engine.addParam("param-name", "param-value", null);

  function readAsyncFile(aFile, aCallback) {
    NetUtil.asyncFetch(aFile, function(inputStream, status) {
      do_check_true(Components.isSuccessCode(status));

      let data = NetUtil.readInputStreamToString(inputStream, inputStream.available());
      aCallback(data);
    });
  }

  let observer = function(aSubject, aTopic, aData) {
    // The sherlock engine file may still be updated because the icon takes
    // time be loaded, therefore, the engine name is checked here.
    aSubject.QueryInterface(Ci.nsIFile);
    if (aTopic == "browser-search-service" &&
        aData == "write-engine-to-disk-complete" &&
        aSubject.leafName == "test-search-engine.xml") {
      Services.obs.removeObserver(observer, aTopic);

      let engineFile = engine.wrappedJSObject._file;

      readAsyncFile(engineFile, function(engineData) {
        do_check_true(engineData.indexOf("param-name") > 0);
        run_next_test();
      });
    }
  }
  Services.obs.addObserver(observer, "browser-search-service", false);
});

function run_test() {
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
  });

  run_next_test();
}
