/* global __LOCATION__ */

function run_test() {
  // skip this test on Windows
  if (mozinfo.os != "win") {
    var testDir = __LOCATION__.parent;
    // create a test file, then symlink it, then check that we think it's a symlink
    var targetFile = testDir.clone();
    targetFile.append("target.txt");
    if (!targetFile.exists()) {
      targetFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
    }

    var link = testDir.clone();
    link.append("link");
    if (link.exists()) {
      link.remove(false);
    }

    var ln = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    ln.initWithPath("/bin/ln");
    var process = Cc["@mozilla.org/process/util;1"].createInstance(
      Ci.nsIProcess
    );
    process.init(ln);
    var args = ["-s", targetFile.path, link.path];
    process.run(true, args, args.length);
    Assert.ok(link.isSymlink());
  }
}
