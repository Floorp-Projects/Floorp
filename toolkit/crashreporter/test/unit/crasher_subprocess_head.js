// enable crash reporting first
let cwd = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);

// get the temp dir
let env = Components.classes["@mozilla.org/process/environment;1"].getService(Components.interfaces.nsIEnvironment);
let _tmpd = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
_tmpd.initWithPath(env.get("XPCSHELL_TEST_TEMP_DIR"));

let crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);

// the crash reporter is already enabled in content processes,
// and setting the minidump path is not allowed
let processType = Components.classes["@mozilla.org/xre/runtime;1"].
      getService(Components.interfaces.nsIXULRuntime).processType;
if (processType == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  crashReporter.enabled = true;
  crashReporter.minidumpPath = _tmpd;
}

let ios = Components.classes["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService);
let protocolHandler = ios.getProtocolHandler("resource")
                        .QueryInterface(Components.interfaces.nsIResProtocolHandler);
let curDirURI = ios.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
Components.utils.import("resource://test/CrashTestUtils.jsm");
let crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
