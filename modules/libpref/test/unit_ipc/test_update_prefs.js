var Ci = Components.interfaces;
var Cc = Components.classes;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function run_test() {
  if (isParentProcess()) {

    do_load_child_test_harness();

    var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

    // these prefs are set after the child has been created.
    pb.setBoolPref("Test.IPC.bool.new", true);
    pb.setIntPref("Test.IPC.int.new", 23);
    pb.setCharPref("Test.IPC.char.new", "hey");

    run_test_in_child("test_observed_prefs.js", testPrefClear);
  }
}

function testPrefClear() {
  var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  pb.clearUserPref("Test.IPC.bool.new");

  sendCommand(
'var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);\n'+
'pb.prefHasUserValue("Test.IPC.bool.new");\n',
    checkWasCleared);
}

function checkWasCleared(existsStr) {
    do_check_eq(existsStr, "false");
    do_test_finished();
}
