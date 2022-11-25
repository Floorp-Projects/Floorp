const CWD = do_get_cwd();
function checkOS(os) {
  const nsILocalFile_ = "nsILocalFile" + os;
  return nsILocalFile_ in Ci && CWD instanceof Ci[nsILocalFile_];
}

const isWin = checkOS("Win");

function run_test() {
  var envVar = isWin ? "USERPROFILE" : "HOME";

  var homeDir = Services.dirsvc.get("Home", Ci.nsIFile);

  var expected = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  expected.initWithPath(Services.env.get(envVar));

  Assert.equal(homeDir.path, expected.path);
}
