createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

add_task(async function() {
  let triggered = {};
  const { Management } = ChromeUtils.import(
    "resource://gre/modules/Extension.jsm",
    null
  );
  for (let event of ["install", "uninstall", "update"]) {
    triggered[event] = false;
    Management.on(event, () => (triggered[event] = true));
  }

  async function expectEvents(expected, fn) {
    let events = Object.keys(expected);
    for (let event of events) {
      triggered[event] = false;
    }

    await fn();
    await new Promise(executeSoon);

    for (let event of events) {
      equal(
        triggered[event],
        expected[event],
        `Event ${event} was${expected[event] ? "" : " not"} triggered`
      );
    }
  }

  await promiseStartupManager();

  const id = "webextension@tests.mozilla.org";

  // Install version 1.0, shouldn't see any events
  await expectEvents({ update: false, uninstall: false }, async () => {
    await promiseInstallWebExtension({
      manifest: {
        version: "1.0",
        applications: { gecko: { id } },
      },
    });
  });

  // Install version 2.0, we should get an update event but not an uninstall
  await expectEvents({ update: true, uninstall: false }, async () => {
    await promiseInstallWebExtension({
      manifest: {
        version: "2.0",
        applications: { gecko: { id } },
      },
    });
  });

  // Install version 3.0 as a temporary addon, we should again get
  // update but not uninstall
  let v3 = createTempWebExtensionFile({
    manifest: {
      version: "3.0",
      applications: { gecko: { id } },
    },
  });

  await expectEvents({ update: true, uninstall: false }, () =>
    AddonManager.installTemporaryAddon(v3)
  );

  // Uninstall the temporary addon, this causes version 2.0 still installed
  // in the profile to be revealed.  Again, this results in an update event.
  let addon = await promiseAddonByID(id);
  await expectEvents({ update: true, uninstall: false }, () =>
    addon.uninstall()
  );

  // Re-install version 3.0 as a temporary addon
  await AddonManager.installTemporaryAddon(v3);

  // Now shut down the addons manager, this should cause the temporary
  // addon to be uninstalled which reveals 2.0 from the profile.
  await expectEvents({ update: true, uninstall: false }, () =>
    promiseShutdownManager()
  );

  // When we start up again we should not see any events
  await expectEvents({ install: false }, () => promiseStartupManager());

  addon = await promiseAddonByID(id);

  // When we uninstall from the profile, the addon is now gone, we should
  // get an uninstall events.
  await expectEvents({ update: false, uninstall: true }, () =>
    addon.uninstall()
  );
});
