/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_hidden() {
  let xpi1 = createTempWebExtensionFile({
    manifest: {
      applications: {
        gecko: {
          id: "privileged@tests.mozilla.org",
        },
      },

      name: "Hidden Extension",
      hidden: true,
    },
  });

  let xpi2 = createTempWebExtensionFile({
    manifest: {
      applications: {
        gecko: {
          id: "unprivileged@tests.mozilla.org",
        },
      },

      name: "Non-Hidden Extension",
      hidden: true,
    },
  });

  await promiseInstallAllFiles([xpi1, xpi2]);

  let [addon1, addon2] = await promiseAddonsByIDs([
    "privileged@tests.mozilla.org",
    "unprivileged@tests.mozilla.org",
  ]);

  ok(addon1.isPrivileged, "Privileged is privileged");
  ok(addon1.hidden, "Privileged extension should be hidden");
  ok(!addon2.isPrivileged, "Unprivileged extension is not privileged");
  ok(!addon2.hidden, "Unprivileged extension should not be hidden");

  await promiseRestartManager();

  [addon1, addon2] = await promiseAddonsByIDs([
    "privileged@tests.mozilla.org",
    "unprivileged@tests.mozilla.org",
  ]);

  ok(addon1.isPrivileged, "Privileged is privileged");
  ok(addon1.hidden, "Privileged extension should be hidden");
  ok(!addon2.isPrivileged, "Unprivileged extension is not privileged");
  ok(!addon2.hidden, "Unprivileged extension should not be hidden");

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      applications: { gecko: { id: "privileged@but-temporary" } },
      hidden: true,
    },
  });
  await extension.startup();
  let tempAddon = extension.addon;
  ok(tempAddon.isPrivileged, "Temporary add-on is privileged");
  ok(
    !tempAddon.hidden,
    "Temporary add-on is not hidden despite being privileged"
  );
  await extension.unload();
});

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_hidden_and_browser_action_props_are_mutually_exclusive() {
    const TEST_CASES = [
      {
        title: "hidden and browser_action",
        manifest: {
          hidden: true,
          browser_action: {},
        },
        expectError: true,
      },
      {
        title: "hidden and page_action",
        manifest: {
          hidden: true,
          page_action: {},
        },
        expectError: true,
      },
      {
        title: "hidden, browser_action and page_action",
        manifest: {
          hidden: true,
          browser_action: {},
          page_action: {},
        },
        expectError: true,
      },
      {
        title: "hidden and no browser_action or page_action",
        manifest: {
          hidden: true,
        },
        expectError: false,
      },
      {
        title: "not hidden and browser_action",
        manifest: {
          hidden: false,
          browser_action: {},
        },
        expectError: false,
      },
      {
        title: "not hidden and page_action",
        manifest: {
          hidden: false,
          page_action: {},
        },
        expectError: false,
      },
      {
        title: "no hidden prop and browser_action",
        manifest: {
          browser_action: {},
        },
        expectError: false,
      },
      {
        title: "hidden and action",
        manifest: {
          manifest_version: 3,
          hidden: true,
          action: {},
        },
        expectError: true,
      },
      {
        title: "hidden, action and page_action",
        manifest: {
          manifest_version: 3,
          hidden: true,
          action: {},
          page_action: {},
        },
        expectError: true,
      },
      {
        title: "no hidden prop and action",
        manifest: {
          manifest_version: 3,
          action: {},
        },
        expectError: false,
      },
      {
        title: "no hidden prop and page_action",
        manifest: {
          page_action: {},
        },
        expectError: false,
      },
      {
        title: "hidden and action but not privileged",
        manifest: {
          manifest_version: 3,
          hidden: true,
          action: {},
        },
        expectError: false,
        isPrivileged: false,
      },
      {
        title: "hidden and browser_action but not privileged",
        manifest: {
          hidden: true,
          browser_action: {},
        },
        expectError: false,
        isPrivileged: false,
      },
      {
        title: "hidden and page_action but not privileged",
        manifest: {
          hidden: true,
          page_action: {},
        },
        expectError: false,
        isPrivileged: false,
      },
    ];

    let count = 0;

    for (const {
      title,
      manifest,
      expectError,
      isPrivileged = true,
    } of TEST_CASES) {
      info(`== ${title} ==`);

      // Thunderbird doesn't have page actions.
      if (manifest.page_action && AppConstants.MOZ_APP_NAME == "thunderbird") {
        continue;
      }

      const extension = ExtensionTestUtils.loadExtension({
        manifest: {
          browser_specific_settings: {
            gecko: {
              id: `${isPrivileged ? "" : "not-"}privileged@ext-${count++}`,
            },
          },
          permissions: ["mozillaAddons"],
          ...manifest,
        },
        background() {
          /* globals browser */
          browser.test.sendMessage("ok");
        },
        isPrivileged,
      });

      if (expectError) {
        await Assert.rejects(
          extension.startup(),
          /Cannot use browser and\/or page actions in hidden add-ons/,
          "expected extension not started"
        );
      } else {
        await extension.startup();
        await extension.awaitMessage("ok");
        await extension.unload();
      }
    }
  }
);
