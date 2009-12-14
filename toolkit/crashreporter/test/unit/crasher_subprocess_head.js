// enable crash reporting first
let cwd = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);
let crashReporter =
  Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
    .getService(Components.interfaces.nsICrashReporter);
crashReporter.enabled = true;
crashReporter.minidumpPath = cwd;
