function isParentProcess() {
  return Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  if (!isParentProcess()) {
    const pb = Services.prefs;
    Assert.equal(pb.getBoolPref("Test.IPC.bool.new"), true);
    Assert.equal(pb.getIntPref("Test.IPC.int.new"), 23);
    Assert.equal(pb.getCharPref("Test.IPC.char.new"), "hey");
  }
}
