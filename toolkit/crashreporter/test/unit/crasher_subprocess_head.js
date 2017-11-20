Components.utils.import("resource://gre/modules/Services.jsm");

// enable crash reporting first
var cwd = Services.dirsvc.get("CurWorkD", Components.interfaces.nsIFile);

// get the temp dir
var env = Components.classes["@mozilla.org/process/environment;1"].getService(Components.interfaces.nsIEnvironment);
var _tmpd = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsIFile);
_tmpd.initWithPath(env.get("XPCSHELL_TEST_TEMP_DIR"));

var crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);

// We need to call this or crash events go in an undefined location.
crashReporter.UpdateCrashEventsDir();

// Setting the minidump path is not allowed in content processes
var processType = Services.appinfo.processType;
if (processType == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  crashReporter.minidumpPath = _tmpd;
}

var protocolHandler = Services.io.getProtocolHandler("resource")
  .QueryInterface(Components.interfaces.nsIResProtocolHandler);
var curDirURI = Services.io.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
Components.utils.import("resource://test/CrashTestUtils.jsm");
var crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
var shouldDelay = false;
