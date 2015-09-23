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

function run_test() {
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function test_batchTask() {
  let [engine1, engine2] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml"},
  ]);

  // Test that files are written correctly.
  let engineFile1 = engine1.wrappedJSObject._file;
  let engineFile2 = engine2.wrappedJSObject._file;
  do_check_true(engineFile1.exists());
  do_check_true(engineFile2.exists());
  do_check_neq(engineFile1.fileSize, 0);
  do_check_neq(engineFile2.fileSize, 0);
});

add_test(function test_addParam() {
  let engine = Services.search.getEngineByName("Test search engine");
  engine.addParam("param-name", "param-value", null);

  function readAsyncFile(aFile, aCallback) {
    NetUtil.asyncFetch({
      uri: NetUtil.newURI(aFile),
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_XMLHTTPREQUEST
    }, function(inputStream, status) {
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
