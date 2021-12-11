/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const ID1 = "addon1@tests.mozilla.org";
const ID2 = "addon2@tests.mozilla.org";
const ID3 = "addon3@tests.mozilla.org";

async function createWebExtension(details) {
  let options = {
    manifest: {
      applications: { gecko: { id: details.id } },

      name: details.name,

      permissions: details.permissions,
    },
  };

  if (details.iconURL) {
    options.manifest.icons = { "64": details.iconURL };
  }

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);

  await AddonTestUtils.manuallyInstall(xpi);
}

add_task(async function test_sideloading() {
  Services.prefs.setIntPref("extensions.autoDisableScopes", 15);
  Services.prefs.setIntPref("extensions.startupScanScopes", 0);

  await createWebExtension({
    id: ID1,
    name: "Test 1",
    userDisabled: true,
    permissions: ["tabs", "https://*/*"],
    iconURL: "foo-icon.png",
  });

  await createWebExtension({
    id: ID2,
    name: "Test 2",
    permissions: ["<all_urls>"],
  });

  await createWebExtension({
    id: ID3,
    name: "Test 3",
    permissions: ["<all_urls>"],
  });

  await promiseStartupManager();

  let sideloaded = await AddonManagerPrivate.getNewSideloads();

  sideloaded.sort((a, b) => a.id.localeCompare(b.id));

  deepEqual(
    sideloaded.map(a => a.id),
    [ID1, ID2, ID3],
    "Got the correct sideload add-ons"
  );

  deepEqual(
    sideloaded.map(a => a.userDisabled),
    [true, true, true],
    "All sideloaded add-ons are disabled"
  );
});

add_task(async function test_getNewSideload_on_invalid_extension() {
  let destDir = AddonTestUtils.profileExtensions.clone();

  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: "@invalid-extension" } },
      name: "Invalid Extension",
    },
  });

  // Create an invalid sideload by creating a file name that doesn't match the
  // actual extension id.
  await OS.File.copy(
    xpi.path,
    OS.Path.join(destDir.path, "@wrong-extension-filename.xpi")
  );

  // Verify that getNewSideloads does not reject or throw when one of the sideloaded extensions
  // is invalid.
  const newSideloads = await AddonManagerPrivate.getNewSideloads();

  const sideloadsInfo = newSideloads
    .sort((a, b) => a.id.localeCompare(b.id))
    .map(({ id, seen, userDisabled, permissions }) => {
      return {
        id,
        seen,
        userDisabled,
        canEnable: Boolean(permissions & AddonManager.PERM_CAN_ENABLE),
      };
    });

  const expectedInfo = { seen: false, userDisabled: true, canEnable: true };

  Assert.deepEqual(
    sideloadsInfo,
    [
      { id: ID1, ...expectedInfo },
      { id: ID2, ...expectedInfo },
      { id: ID3, ...expectedInfo },
    ],
    "Got the expected sideloaded extensions"
  );
});
