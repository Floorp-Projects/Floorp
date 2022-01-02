// Test that calling Services.processtools.kill doesn't create a crash report.
add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_kill.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // Let's launch a child process and kill it (from within, it's simpler).

  do_load_child_test_harness();

  // Setting the minidump path won't work in the child, so we need to do
  // that here.
  let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
    Ci.nsICrashReporter
  );
  crashReporter.minidumpPath = do_get_tempdir();
  let headfile = do_get_file("../unit/crasher_subprocess_head.js");
  const CRASH_THEN_WAIT =
    "const ProcessTools = Cc['@mozilla.org/processtools-service;1'].getService(Ci.nsIProcessToolsService);\
     console.log('Child process commiting ritual self-sacrifice');\
     ProcessTools.kill(ProcessTools.pid);\
     console.error('Oops, I should be dead');\
     while (true) {} ;";
  do_get_profile();
  await makeFakeAppDir();
  await sendCommandAsync('load("' + headfile.path.replace(/\\/g, "/") + '");');
  await sendCommandAsync(CRASH_THEN_WAIT);

  // Let's wait a little to give the child process a chance to create a minidump.
  let { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 100));

  // Now make sure that we have no minidump.
  let minidump = getMinidump();
  Assert.equal(
    minidump,
    null,
    `There should be no minidump ${minidump == null ? "null" : minidump.path}`
  );
});
