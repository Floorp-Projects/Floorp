// enable crash reporting first
let cwd = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);
let crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);
crashReporter.enabled = true;
crashReporter.minidumpPath = cwd;
let cd = cwd.clone();
cd.append("components");
cd.append("testcrasher.manifest");
Components.manager instanceof Components.interfaces.nsIComponentRegistrar;
Components.manager.autoRegister(cd);
let crashType = Components.interfaces.nsITestCrasher.CRASH_INVALID_POINTER_DEREF;
