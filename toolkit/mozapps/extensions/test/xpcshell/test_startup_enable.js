createAppInfo("xpcshell@tessts.mozilla.org", "XPCShell", "1", "1");
BootstrapMonitor.init();

// Test that enabling an extension during startup generates the
// proper reason for startup().
add_task(async function test_startup_enable() {
  const ID = "compat@tests.mozilla.org";

  await promiseStartupManager();

  await promiseInstallWebExtension({
    manifest: {
      applications: {
        gecko: {
          id: ID,
          strict_min_version: "1",
          strict_max_version: "1",
        },
      },
    },
  });

  BootstrapMonitor.checkInstalled(ID);
  BootstrapMonitor.checkStarted(ID);
  let { reason } = BootstrapMonitor.started.get(ID);
  equal(
    reason,
    BOOTSTRAP_REASONS.ADDON_INSTALL,
    "Startup reason is ADDON_INSTALL at install"
  );

  gAppInfo.platformVersion = "2";
  await promiseRestartManager("2");
  BootstrapMonitor.checkInstalled(ID);
  BootstrapMonitor.checkNotStarted(ID);

  gAppInfo.platformVersion = "1";
  await promiseRestartManager("1");
  BootstrapMonitor.checkInstalled(ID);
  BootstrapMonitor.checkStarted(ID);
  ({ reason } = BootstrapMonitor.started.get(ID));
  equal(
    reason,
    BOOTSTRAP_REASONS.ADDON_ENABLE,
    "Startup reason is ADDON_ENABLE when re-enabled at startup"
  );
});
