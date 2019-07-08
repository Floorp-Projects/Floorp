"use strict";

add_task(async function test_XPIStates_invalid_paths() {
  let { path } = gAddonStartup;

  let startupDatasets = [
    {
      "app-profile": {
        addons: {
          "xpcshell-something-or-other@mozilla.org": {
            bootstrapped: true,
            dependencies: [],
            enabled: true,
            hasEmbeddedWebExtension: false,
            lastModifiedTime: 1,
            path: "xpcshell-something-or-other@mozilla.org",
            version: "0.0.0",
          },
        },
        checkStartupModifications: true,
        path: "/home/xpcshell/.mozilla/firefox/default/extensions",
      },
    },
    {
      "app-profile": {
        addons: {
          "xpcshell-something-or-other@mozilla.org": {
            bootstrapped: true,
            dependencies: [],
            enabled: true,
            hasEmbeddedWebExtension: false,
            lastModifiedTime: 1,
            path: "xpcshell-something-or-other@mozilla.org",
            version: "0.0.0",
          },
        },
        checkStartupModifications: true,
        path:
          "c:\\Users\\XpcShell\\Application Data\\Mozilla Firefox\\Profiles\\meh",
      },
    },
    {
      "app-profile": {
        addons: {
          "xpcshell-something-or-other@mozilla.org": {
            bootstrapped: true,
            dependencies: [],
            enabled: true,
            hasEmbeddedWebExtension: false,
            lastModifiedTime: 1,
            path: "/home/xpcshell/my-extensions/something-or-other",
            version: "0.0.0",
          },
        },
        checkStartupModifications: true,
        path: "/home/xpcshell/.mozilla/firefox/default/extensions",
      },
    },
    {
      "app-profile": {
        addons: {
          "xpcshell-something-or-other@mozilla.org": {
            bootstrapped: true,
            dependencies: [],
            enabled: true,
            hasEmbeddedWebExtension: false,
            lastModifiedTime: 1,
            path: "c:\\Users\\XpcShell\\my-extensions\\something-or-other",
            version: "0.0.0",
          },
        },
        checkStartupModifications: true,
        path:
          "c:\\Users\\XpcShell\\Application Data\\Mozilla Firefox\\Profiles\\meh",
      },
    },
  ];

  for (let startupData of startupDatasets) {
    let data = new TextEncoder().encode(JSON.stringify(startupData));

    await OS.File.writeAtomic(path, data, { compression: "lz4" });

    try {
      let result = aomStartup.readStartupData();
      info(`readStartupData() returned ${JSON.stringify(result)}`);
    } catch (e) {
      // We don't care if this throws, only that it doesn't crash.
      info(`readStartupData() threw: ${e}`);
      equal(
        e.result,
        Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH,
        "Got expected error code"
      );
    }
  }
});
