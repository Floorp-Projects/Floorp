add_task(async function test() {
  SimpleTest.doesThrow(function () {
    getTestFilePath("/browser_getTestFile.js");
  }, "getTestFilePath rejects absolute paths");

  await Promise.all([
    IOUtils.exists(getTestFilePath("browser_getTestFile.js")).then(function (
      exists
    ) {
      ok(exists, "getTestFilePath consider the path as being relative");
    }),

    IOUtils.exists(getTestFilePath("./browser_getTestFile.js")).then(function (
      exists
    ) {
      ok(exists, "getTestFilePath also accepts explicit relative path");
    }),

    IOUtils.exists(getTestFilePath("./browser_getTestFileTypo.xul")).then(
      function (exists) {
        ok(!exists, "getTestFilePath do not throw if the file doesn't exists");
      }
    ),

    IOUtils.readUTF8(getTestFilePath("test-dir/test-file")).then(function (
      content
    ) {
      is(content, "foo\n", "getTestFilePath can reach sub-folder files 1/2");
    }),

    IOUtils.readUTF8(getTestFilePath("./test-dir/test-file")).then(function (
      content
    ) {
      is(content, "foo\n", "getTestFilePath can reach sub-folder files 2/2");
    }),
  ]);
});
