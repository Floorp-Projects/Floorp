var Ci = Components.interfaces;
var Cc = Components.classes;
const NS_OS_TEMP_DIR = "TmpD";

const CWD = do_get_cwd();

var hiddenUnixFile;
function createUNIXHiddenFile() {
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var tmpDir = dirSvc.get(NS_OS_TEMP_DIR, Ci.nsIFile);
  hiddenUnixFile = tmpDir.clone();
  hiddenUnixFile.append(".foo");
  // we don't care if this already exists because we don't care
  // about the file's contents (just the name)
  if (!hiddenUnixFile.exists())
    hiddenUnixFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  return hiddenUnixFile.exists();
}

function run_test() {
  // Skip this test on Windows
  if (mozinfo.os == "win")
    return;

  do_check_true(createUNIXHiddenFile());
  do_check_true(hiddenUnixFile.isHidden());
}

