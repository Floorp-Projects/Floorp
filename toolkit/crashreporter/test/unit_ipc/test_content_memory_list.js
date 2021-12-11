// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  var is_win7_or_newer = false;
  var ph = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  );
  var match = ph.userAgent.match(/Windows NT (\d+).(\d+)/);
  if (
    match &&
    (parseInt(match[1]) > 6 ||
      (parseInt(match[1]) == 6 && parseInt(match[2]) >= 1))
  ) {
    is_win7_or_newer = true;
  }

  await do_content_crash(null, function(mdump, extra) {
    Assert.ok(mdump.exists());
    Assert.ok(mdump.fileSize > 0);
    if (is_win7_or_newer) {
      Assert.ok(
        CrashTestUtils.dumpHasStream(
          mdump.path,
          CrashTestUtils.MD_MEMORY_INFO_LIST_STREAM
        )
      );
    }
  });
});
