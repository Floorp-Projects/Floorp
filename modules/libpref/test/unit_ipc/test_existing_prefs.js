const Ci = Components.interfaces;
const Cc = Components.classes;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function run_test() {
  if (isParentProcess() == false) {

    do_load_child_test_harness();
 
    var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    pb.setBoolPref("Test.IPC.bool.new", true);
    pb.setIntPref("Test.IPC.int.new", 23);
    pb.setCharPref("Test.IPC.char.new", "hey");

    run_test_in_child("test_observed_prefs.js");
  }
}