add_task(async function test() {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  let decoder = new TextDecoder();

  SimpleTest.doesThrow(function() {
    getTestFilePath("/browser_getTestFile.js");
  }, "getTestFilePath rejects absolute paths");

  await Promise.all([
    OS.File.exists(getTestFilePath("browser_getTestFile.js")).then(function(
      exists
    ) {
      ok(exists, "getTestFilePath consider the path as being relative");
    }),

    OS.File.exists(getTestFilePath("./browser_getTestFile.js")).then(function(
      exists
    ) {
      ok(exists, "getTestFilePath also accepts explicit relative path");
    }),

    OS.File.exists(getTestFilePath("./browser_getTestFileTypo.xul")).then(
      function(exists) {
        ok(!exists, "getTestFilePath do not throw if the file doesn't exists");
      }
    ),

    OS.File.read(getTestFilePath("test-dir/test-file")).then(function(array) {
      is(
        decoder.decode(array),
        "foo\n",
        "getTestFilePath can reach sub-folder files 1/2"
      );
    }),

    OS.File.read(getTestFilePath("./test-dir/test-file")).then(function(array) {
      is(
        decoder.decode(array),
        "foo\n",
        "getTestFilePath can reach sub-folder files 2/2"
      );
    }),
  ]);
});
