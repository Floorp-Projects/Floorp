// enable crash reporting first
var cwd = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);

// get the temp dir
var env = Components.classes["@mozilla.org/process/environment;1"].getService(Components.interfaces.nsIEnvironment);
var _tmpd = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
_tmpd.initWithPath(env.get("XPCSHELL_TEST_TEMP_DIR"));

var crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);

// We need to call this or crash events go in an undefined location.
crashReporter.UpdateCrashEventsDir();

// Setting the minidump path is not allowed in content processes
var processType = Components.classes["@mozilla.org/xre/runtime;1"].
      getService(Components.interfaces.nsIXULRuntime).processType;
if (processType == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  crashReporter.minidumpPath = _tmpd;
}

var ios = Components.classes["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService);
var protocolHandler = ios.getProtocolHandler("resource")
                        .QueryInterface(Components.interfaces.nsIResProtocolHandler);
var curDirURI = ios.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
Components.utils.import("resource://test/CrashTestUtils.jsm");
var crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
var shouldDelay = false;
