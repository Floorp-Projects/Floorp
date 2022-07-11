const NS_OS_TEMP_DIR = "TmpD";

var hiddenUnixFile;
function createUNIXHiddenFile() {
  var tmpDir = Services.dirsvc.get(NS_OS_TEMP_DIR, Ci.nsIFile);
  hiddenUnixFile = tmpDir.clone();
  hiddenUnixFile.append(".foo");
  // we don't care if this already exists because we don't care
  // about the file's contents (just the name)
  if (!hiddenUnixFile.exists()) {
    hiddenUnixFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  }
  return hiddenUnixFile.exists();
}

function run_test() {
  // Skip this test on Windows
  if (mozinfo.os == "win") {
    return;
  }

  Assert.ok(createUNIXHiddenFile());
  Assert.ok(hiddenUnixFile.isHidden());
}
