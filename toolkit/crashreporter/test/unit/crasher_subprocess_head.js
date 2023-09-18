// enable crash reporting first
var cwd = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

// get the temp dir
var _tmpd = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
_tmpd.initWithPath(Services.env.get("XPCSHELL_TEST_TEMP_DIR"));

// Allow `crashReporter` to be used as an alias in the tests.
var crashReporter = Services.appinfo;

// We need to call this or crash events go in an undefined location.
Services.appinfo.UpdateCrashEventsDir();

// Setting the minidump path is not allowed in content processes
var processType = Services.appinfo.processType;
if (processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  Services.appinfo.minidumpPath = _tmpd;
}

var protocolHandler = Services.io
  .getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler);
var curDirURI = Services.io.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
const { CrashTestUtils } = ChromeUtils.importESModule(
  "resource://test/CrashTestUtils.sys.mjs"
);
var crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
var shouldDelay = false;

// Turn PHC on so that the PHC tests work.
CrashTestUtils.enablePHC();
