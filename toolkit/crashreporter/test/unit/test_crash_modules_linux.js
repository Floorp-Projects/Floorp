add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_modules.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function () {
      crashType = CrashTestUtils.CRASH_ABORT;
    },
    async function (mdump, extra, extraFile) {
      runMinidumpAnalyzer(mdump);

      // Refresh updated extra data
      extra = await IOUtils.readJSON(extraFile.path);

      // Check modules' versions
      const modules = extra.StackTraces.modules;
      const version_regexp = /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/;

      for (let module of modules) {
        console.debug("module", module);
        console.debug("version => ", module.version);
        console.debug("version regex => ", version_regexp.exec(module.version));
        Assert.notEqual(version_regexp.exec(module.version), null);
      }
    },
    // process will exit with a zero exit status
    true
  );
});
