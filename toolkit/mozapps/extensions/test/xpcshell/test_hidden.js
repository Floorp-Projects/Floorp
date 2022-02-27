/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_hidden() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");
  AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

  await promiseStartupManager();

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
  ok(!tempAddon.hidden, "Temporary add-on is not hidden despite privilige");
  await extension.unload();

  await promiseShutdownManager();
});
