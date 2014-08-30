function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_oom.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // This was shamelessly copied and stripped down from do_get_profile() in
  // head.js so that nsICrashReporter::saveMemoryReport can use a profile
  // within the crasher subprocess.

  do_crash(
   function() {
      let Cc = Components.classes;
      let Ci = Components.interfaces;

      let env = Cc["@mozilla.org/process/environment;1"]
                  .getService(Ci.nsIEnvironment);
      let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
      let file = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile);
      file.initWithPath(profd);

      let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                     .getService(Ci.nsIProperties);
      let provider = {
        getFile: function(prop, persistent) {
          persistent.value = true;
              if (prop == "ProfD" || prop == "ProfLD" || prop == "ProfDS" ||
              prop == "ProfLDS" || prop == "TmpD") {
            return file.clone();
          }
          throw Components.results.NS_ERROR_FAILURE;
        },
        QueryInterface: function(iid) {
          if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
              iid.equals(Ci.nsISupports)) {
            return this;
          }
          throw Components.results.NS_ERROR_NO_INTERFACE;
        }
      };
      dirSvc.QueryInterface(Ci.nsIDirectoryService)
            .registerProvider(provider);

      crashReporter.saveMemoryReport();
    },
    function(mdump, extra) {
      do_check_eq(extra.ContainsMemoryReport, "1");
    },
    true);
}
