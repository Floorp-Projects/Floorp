const Ci = Components.interfaces;
const Cc = Components.classes;

const CWD = do_get_cwd();
function checkOS(os) {
  const nsILocalFile_ = "nsILocalFile" + os;
  return nsILocalFile_ in Components.interfaces &&
         CWD instanceof Components.interfaces[nsILocalFile_];
}

const isWin = checkOS("Win");

function run_test() {
  var envVar = isWin ? "USERPROFILE" : "HOME";

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var homeDir = dirSvc.get("Home", Ci.nsIFile);

  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  var expected = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  expected.initWithPath(env.get(envVar));

  do_check_eq(homeDir.path, expected.path);
}
