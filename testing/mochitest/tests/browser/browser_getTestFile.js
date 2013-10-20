function test() {
  let {Promise} = Components.utils.import("resource://gre/modules/commonjs/sdk/core/promise.js");
  Components.utils.import("resource://gre/modules/osfile.jsm");
  let decoder = new TextDecoder();

  waitForExplicitFinish();

  SimpleTest.doesThrow(function () {
    getTestFilePath("/browser_getTestFile.js")
  }, "getTestFilePath rejects absolute paths");

  Promise.all([
    OS.File.exists(getTestFilePath("browser_getTestFile.js"))
      .then(function (exists) {
        ok(exists, "getTestFilePath consider the path as being relative");
      }),

    OS.File.exists(getTestFilePath("./browser_getTestFile.js"))
      .then(function (exists) {
        ok(exists, "getTestFilePath also accepts explicit relative path");
      }),

    OS.File.exists(getTestFilePath("./browser_getTestFileTypo.xul"))
      .then(function (exists) {
        ok(!exists, "getTestFilePath do not throw if the file doesn't exists");
      }),

    OS.File.read(getTestFilePath("test-dir/test-file"))
      .then(function (array) {
        is(decoder.decode(array), "foo\n", "getTestFilePath can reach sub-folder files 1/2");
      }),

    OS.File.read(getTestFilePath("./test-dir/test-file"))
      .then(function (array) {
        is(decoder.decode(array), "foo\n", "getTestFilePath can reach sub-folder files 2/2");
      })

  ]).then(function () {
    finish();
  }, function (error) {
    ok(false, error);
    finish();
  });
}
