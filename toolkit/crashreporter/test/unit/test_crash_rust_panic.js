function run_test() {
  // Try crashing with a Rust panic
  do_crash(function() {
             Components.classes["@mozilla.org/xpcom/debug;1"].getService(Components.interfaces.nsIDebug2).rustPanic("OH NO");
           },
           function(mdump, extra) {
             Assert.equal(extra.MozCrashReason, "OH NO");
           },
          // process will exit with a zero exit status
          true);
}
