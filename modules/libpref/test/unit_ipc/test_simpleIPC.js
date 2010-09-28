const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function verify_existing_prefs() {
  const Ci = Components.interfaces;
  const Cc = Components.classes;
  var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  
  do_check_eq(pb.getBoolPref("Test.IPC.bool"), true);
  do_check_eq(pb.getIntPref("Test.IPC.int"), 23);
  do_check_eq(pb.getCharPref("Test.IPC.char"), "hey");
  do_test_finished();
  _do_main();
 }
 
function verify_observed_prefs() {
  const Ci = Components.interfaces;
  const Cc = Components.classes;
  var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  do_test_pending();
  do_check_eq(pb.getBoolPref("Test.IPC.bool.new"), true);
  do_check_eq(pb.getIntPref("Test.IPC.int.new"), 23);
  do_check_eq(pb.getCharPref("Test.IPC.char.new"), "hey");
  do_test_finished();
  _do_main();
}

function run_test() {
  if (isParentProcess()) {
    var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

    pb.setBoolPref("Test.IPC.bool", true);
    pb.setIntPref("Test.IPC.int", 23);
    pb.setCharPref("Test.IPC.char", "hey");
    
    do_load_child_test_harness();

    do_test_pending();
    sendCommand(verify_existing_prefs.toString(), do_test_finished);

    // these prefs are set after the child has been created.
    pb.setBoolPref("Test.IPC.bool.new", true);
    pb.setIntPref("Test.IPC.int.new", 23);
    pb.setCharPref("Test.IPC.char.new", "hey");
    
    do_test_pending();
    sendCommand(verify_existing_prefs.toString(), do_test_finished);
  }
  else {
  }
}