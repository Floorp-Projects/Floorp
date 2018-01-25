Cu.import("resource://gre/modules/Services.jsm");

let executableFile = Services.dirsvc.get("CurProcD", Ci.nsIFile);
executableFile.append("xpcshell.exe");
function run_test() {
  let quote = '"'; // Windows' cmd processor doesn't actually use single quotes.
  for (let suffix of ["", " -osint", ` --blah "%PROGRAMFILES%"`]) {
    let cmdline = quote + executableFile.path + quote + suffix;
    info(`Testing with ${cmdline}`);
    let f = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFileWin);
    f.initWithCommandLine(cmdline);
    Assert.equal(f.path, executableFile.path, "Should be able to recover executable path");
  }

  let f = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFileWin);
  f.initWithCommandLine("%ComSpec% -c echo 'hi'");
  let cmd = Services.dirsvc.get("SysD", Ci.nsIFile);
  cmd.append("cmd.exe");
  Assert.equal(f.path, cmd.path, "Should be able to replace env vars.");
}
