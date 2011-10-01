// enable crash reporting first
let cwd = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);

let crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);

// the crash reporter is already enabled in content processes,
// and setting the minidump path is not allowed
let processType = Components.classes["@mozilla.org/xre/runtime;1"].
      getService(Components.interfaces.nsIXULRuntime).processType;
if (processType == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  crashReporter.enabled = true;
  crashReporter.minidumpPath = cwd;
}

let ios = Components.classes["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService);
let protocolHandler = ios.getProtocolHandler("resource")
                        .QueryInterface(Components.interfaces.nsIResProtocolHandler);
let curDirURI = ios.newFileURI(cwd);
protocolHandler.setSubstitution("test", curDirURI);
Components.utils.import("resource://test/CrashTestUtils.jsm");
let crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
