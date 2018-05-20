/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  var nsIFile = Ci.nsIFile;
  var root = Cc["@mozilla.org/file/local;1"].
              createInstance(nsIFile);

  // copied from http://mxr.mozilla.org/mozilla-central/source/image/test/unit/test_imgtools.js#135
  // nsIXULRuntime.OS doesn't seem to be available in xpcshell, so we'll use
  // this as a kludgy way to figure out if we're running on Windows.
  if (mozinfo.os == "win") {
    root.initWithPath("\\\\.");
  } else {
    return; // XXX disabled, since this causes intermittent failures on Mac (bug 481369).
    // root.initWithPath("/");
  }
  var drives = root.directoryEntries;
  Assert.ok(drives.hasMoreElements());
  while (drives.hasMoreElements()) {
    var newPath = drives.nextFile.path;
    Assert.equal(newPath.indexOf("\0"), -1);
  }
}
