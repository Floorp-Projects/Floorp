var Ci = Components.interfaces;
var Cc = Components.classes;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function run_test() {
  if (isParentProcess() == false) {
      var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      Assert.equal(pb.getBoolPref("Test.IPC.bool.new"), true);
      Assert.equal(pb.getIntPref("Test.IPC.int.new"), 23);
      Assert.equal(pb.getCharPref("Test.IPC.char.new"), "hey");
  }
}
