function isParentProcess() {
  return Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  if (!isParentProcess()) {
    const pb = Services.prefs;
    pb.setBoolPref("Test.IPC.bool", true);
    pb.setIntPref("Test.IPC.int", 23);
    pb.setCharPref("Test.IPC.char", "hey");

    run_test_in_child("test_existing_prefs.js");
  }
}
