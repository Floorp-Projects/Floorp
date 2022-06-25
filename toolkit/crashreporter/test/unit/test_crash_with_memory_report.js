add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_oom.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // This was shamelessly copied and stripped down from do_get_profile() in
  // head.js so that nsICrashReporter::saveMemoryReport can use a profile
  // within the crasher subprocess.

  await do_crash(
    function() {
      // Delay crashing so that the memory report has time to complete.
      shouldDelay = true;

      let env = Cc["@mozilla.org/process/environment;1"].getService(
        Ci.nsIEnvironment
      );
      let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(profd);

      let provider = {
        getFile(prop, persistent) {
          persistent.value = true;
          if (
            prop == "ProfD" ||
            prop == "ProfLD" ||
            prop == "ProfDS" ||
            prop == "ProfLDS" ||
            prop == "TmpD"
          ) {
            return file.clone();
          }
          throw Components.Exception("", Cr.NS_ERROR_FAILURE);
        },
        QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
      };
      Services.dirsvc
        .QueryInterface(Ci.nsIDirectoryService)
        .registerProvider(provider);

      crashReporter.saveMemoryReport();
    },
    function(mdump, extra, extrafile, memoryfile) {
      Assert.ok(memoryfile.exists());
    },
    true
  );
});
