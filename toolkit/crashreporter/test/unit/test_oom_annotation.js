add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_oom.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_OOM;
      crashReporter.annotateCrashReport("TestKey", "Yes");
    },
    function(mdump, extra) {
      const { AppConstants } = ChromeUtils.import(
        "resource://gre/modules/AppConstants.jsm"
      );
      Assert.equal(extra.TestKey, "Yes");

      // A list of pairs [annotation name, must be > 0]
      let annotations;
      switch (AppConstants.platform) {
        case "win":
          annotations = [
            ["OOMAllocationSize", true],
            ["SystemMemoryUsePercentage", false],
            ["TotalVirtualMemory", true],
            ["AvailableVirtualMemory", false],
            ["TotalPageFile", false],
            ["AvailablePageFile", false],
            ["TotalPhysicalMemory", true],
            ["AvailablePhysicalMemory", false],
          ];
          break;
        case "linux":
          annotations = [
            ["OOMAllocationSize", true],
            ["AvailablePageFile", false],
            ["AvailablePhysicalMemory", false],
            ["AvailableSwapMemory", false],
            ["AvailableVirtualMemory", false],
            ["TotalPageFile", false],
            ["TotalPhysicalMemory", true],
          ];
          break;
        case "macosx":
          annotations = [
            ["OOMAllocationSize", true],
            ["AvailablePhysicalMemory", false],
            ["AvailableSwapMemory", false],
            ["PurgeablePhysicalMemory", false],
            ["TotalPhysicalMemory", true],
          ];
          break;
        default:
          annotations = [];
      }
      for (let [label, shouldBeGreaterThanZero] of annotations) {
        Assert.ok(label in extra, `Annotation ${label} is present`);

        // All these annotations should represent non-negative numbers.
        // A few of them (e.g. physical memory) are guaranteed to be positive.
        if (shouldBeGreaterThanZero) {
          Assert.ok(Number(extra[label]) > 0);
        } else {
          Assert.ok(Number(extra[label]) >= 0);
        }
      }
    }
  );
});
