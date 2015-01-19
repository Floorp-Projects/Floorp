createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

function run_test() {
  run_next_test();
}

add_task(function* () {
  equal(AddonManager.isReady, false, "isReady should be false before startup");

  let gotStartupEvent = false;
  let gotShutdownEvent = false;
  let listener = {
    onStartup() {
      gotStartupEvent = true;
    },
    onShutdown() {
      gotShutdownEvent = true;
    },
  };
  AddonManager.addManagerListener(listener);

  do_print("Starting manager...");
  startupManager();
  equal(AddonManager.isReady, true, "isReady should be true after startup");
  equal(gotStartupEvent, true, "Should have seen onStartup event after startup");
  equal(gotShutdownEvent, false, "Should not have seen onShutdown event before shutdown");

  gotStartupEvent = false;
  gotShutdownEvent = false;

  do_print("Shutting down manager...");
  let shutdownPromise = promiseShutdownManager();
  equal(AddonManager.isReady, false, "isReady should be false when shutdown commences");
  yield shutdownPromise;

  equal(AddonManager.isReady, false, "isReady should be false after shutdown");
  equal(gotStartupEvent, false, "Should not have seen onStartup event after shutdown");
  equal(gotShutdownEvent, true, "Should have seen onShutdown event after shutdown");

  AddonManager.addManagerListener(listener);
  gotStartupEvent = false;
  gotShutdownEvent = false;

  do_print("Starting manager again...");
  startupManager();
  equal(AddonManager.isReady, true, "isReady should be true after repeat startup");
  equal(gotStartupEvent, true, "Should have seen onStartup event after repeat startup");
  equal(gotShutdownEvent, false, "Should not have seen onShutdown event before shutdown, following repeat startup");
});
