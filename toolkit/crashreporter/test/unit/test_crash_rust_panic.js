add_task(async function run_test() {
  // Try crashing with a Rust panic
  await do_crash(
    function() {
      Cc["@mozilla.org/xpcom/debug;1"]
        .getService(Ci.nsIDebug2)
        .rustPanic("OH NO");
    },
    function(mdump, extra) {
      Assert.equal(extra.MozCrashReason, "OH NO");
    },
    // process will exit with a zero exit status
    true
  );
});
