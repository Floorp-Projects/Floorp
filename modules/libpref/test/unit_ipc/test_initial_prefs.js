var Ci = Components.interfaces;
var Cc = Components.classes;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function run_test() {
  if (isParentProcess() == false) {
      var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      pb.setBoolPref("Test.IPC.bool", true);
      pb.setIntPref("Test.IPC.int", 23);
      pb.setCharPref("Test.IPC.char", "hey");

      run_test_in_child("test_existing_prefs.js");
  }
}
