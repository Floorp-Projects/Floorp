function isParentProcess() {
  let appInfo = Cc["@mozilla.org/xre/app-info;1"];
  return (
    !appInfo ||
    Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

function run_test() {
  if (!isParentProcess()) {
    do_load_child_test_harness();

    var pb = Services.prefs;
    pb.setBoolPref("Test.IPC.bool.new", true);
    pb.setIntPref("Test.IPC.int.new", 23);
    pb.setCharPref("Test.IPC.char.new", "hey");

    run_test_in_child("test_observed_prefs.js");
  }
}
