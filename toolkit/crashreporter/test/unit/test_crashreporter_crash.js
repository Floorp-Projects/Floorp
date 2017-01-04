function run_test() {
  var is_win7_or_newer = false;
  var is_windows = false;
  var ph = Components.classes["@mozilla.org/network/protocol;1?name=http"]
             .getService(Components.interfaces.nsIHttpProtocolHandler);
  var match = ph.userAgent.match(/Windows NT (\d+).(\d+)/);
  if (match) {
      is_windows = true;
  }
  if (match && (parseInt(match[1]) > 6 ||
                parseInt(match[1]) == 6 && parseInt(match[2]) >= 1)) {
      is_win7_or_newer = true;
  }

  // try a basic crash
  do_crash(null, function(mdump, extra) {
             do_check_true(mdump.exists());
             do_check_true(mdump.fileSize > 0);
             do_check_true('StartupTime' in extra);
             do_check_true('CrashTime' in extra);
             do_check_true(CrashTestUtils.dumpHasStream(mdump.path, CrashTestUtils.MD_THREAD_LIST_STREAM));
             do_check_true(CrashTestUtils.dumpHasInstructionPointerMemory(mdump.path));
             if (is_windows) {
               ['SystemMemoryUsePercentage', 'TotalVirtualMemory', 'AvailableVirtualMemory',
                'AvailablePageFile', 'AvailablePhysicalMemory'].forEach(function(prop) {
                  do_check_true(/^\d+$/.test(extra[prop].toString()));
               });
             }
             if (is_win7_or_newer)
               do_check_true(CrashTestUtils.dumpHasStream(mdump.path, CrashTestUtils.MD_MEMORY_INFO_LIST_STREAM));
           });

  // check setting some basic data
  do_crash(function() {
             crashReporter.annotateCrashReport("TestKey", "TestValue");
             crashReporter.annotateCrashReport("\u2665", "\u{1F4A9}");
             crashReporter.appendAppNotesToCrashReport("Junk");
             crashReporter.appendAppNotesToCrashReport("MoreJunk");
             // TelemetrySession setup will trigger the session annotation
             let scope = {};
             Components.utils.import("resource://gre/modules/TelemetryController.jsm", scope);
             scope.TelemetryController.testSetup();
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
             do_check_eq(extra["\u2665"], "\u{1F4A9}");
             do_check_eq(extra.Notes, "JunkMoreJunk");
             do_check_true(!("TelemetrySessionId" in extra));
           });
}
