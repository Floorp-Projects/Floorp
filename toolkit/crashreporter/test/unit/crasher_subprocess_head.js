const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// enable crash reporting first
var cwd = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

// get the temp dir
var env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
var _tmpd = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
_tmpd.initWithPath(env.get("XPCSHELL_TEST_TEMP_DIR"));

var crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
  Ci.nsICrashReporter
);

// We need to call this or crash events go in an undefined location.
crashReporter.UpdateCrashEventsDir();

// Setting the minidump path is not allowed in content processes
var processType = Services.appinfo.processType;
if (processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  crashReporter.minidumpPath = _tmpd;
}

var protocolHandler = Services.io
  .getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler);
var curDirURI = Services.io.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
const { CrashTestUtils } = ChromeUtils.import(
  "resource://test/CrashTestUtils.jsm"
);
var crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
var shouldDelay = false;
