/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_content_rust_panic.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a Rust panic
  do_triggered_content_crash(
    function() {
      Components.classes["@mozilla.org/xpcom/debug;1"]
                .getService(Components.interfaces.nsIDebug2)
                .rustPanic("OH NO");
    },
    function(mdump, extra) {
      do_check_eq(extra.MozCrashReason, "OH NO");
    }
  );
}
