const { Log } = ChromeUtils.importESModule(
  "resource://gre/modules/Log.sys.mjs"
);

function run_test() {
  // Verify error with double extensions are handled correctly.

  const nsIExceptionTrace = Log.stackTrace({
    location: {
      filename: "chrome://foo/bar/file.sys.mjs",
      lineNumber: 10,
      name: "func",
      caller: null,
    },
  });

  Assert.equal(nsIExceptionTrace, "Stack trace: func()@file.sys.mjs:10");

  const JSExceptionTrace = Log.stackTrace({
    stack: "func2@chrome://foo/bar/file.sys.mjs:10:2",
  });

  Assert.equal(JSExceptionTrace, "JS Stack trace: func2@file.sys.mjs:10:2");
}
