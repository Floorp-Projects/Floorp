/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_rust_panic.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // Try crashing with a Rust panic
  await do_triggered_content_crash(
    function() {
      Cc["@mozilla.org/xpcom/debug;1"]
        .getService(Ci.nsIDebug2)
        .rustPanic("OH NO");
    },
    function(mdump, extra) {
      Assert.equal(extra.MozCrashReason, "OH NO");
    }
  );
});
