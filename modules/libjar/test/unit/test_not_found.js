// Should report file not found on non-existent files

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
const path = "data/test_bug333423.zip";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var spec = "jar:" + ios.newFileURI(do_get_file(path)).spec + "!/";
  var channel = ios.newChannel2(spec + "file_that_isnt_in.archive",
                                null,
                                null,
                                null,      // aLoadingNode
                                Services.scriptSecurityManager.getSystemPrincipal(),
                                null,      // aTriggeringPrincipal
                                Ci.nsILoadInfo.SEC_NORMAL,
                                Ci.nsIContentPolicy.TYPE_OTHER);
  try {
    instr = channel.open();
    do_throw("Failed to report that file doesn't exist")
  } catch (e) {
      do_check_true(e.name == "NS_ERROR_FILE_NOT_FOUND")
  }
}
