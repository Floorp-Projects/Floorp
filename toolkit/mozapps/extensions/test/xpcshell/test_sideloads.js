/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

async function createWebExtension(details) {
  let options = {
    manifest: {
      applications: {gecko: {id: details.id}},

      name: details.name,

      permissions: details.permissions,
    },
  };

  if (details.iconURL) {
    options.manifest.icons = {"64": details.iconURL};
  }

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);

  await AddonTestUtils.manuallyInstall(xpi);
}

async function createXULExtension(details) {
  let xpi = AddonTestUtils.createTempXPIFile({
    "install.rdf": {
      id: details.id,
      name: details.name,
      version: "0.1",
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "0",
        maxVersion: "*",
      }],
    },
  });

  await AddonTestUtils.manuallyInstall(xpi);
}

add_task(async function test_sideloading() {
  Services.prefs.setIntPref("extensions.autoDisableScopes", 15);
  Services.prefs.setIntPref("extensions.startupScanScopes", 0);

  const ID1 = "addon1@tests.mozilla.org";
  await createWebExtension({
    id: ID1,
    name: "Test 1",
    userDisabled: true,
    permissions: ["history", "https://*/*"],
    iconURL: "foo-icon.png",
  });

  const ID2 = "addon2@tests.mozilla.org";
  await createXULExtension({
    id: ID2,
    name: "Test 2",
  });

  const ID3 = "addon3@tests.mozilla.org";
  await createWebExtension({
    id: ID3,
    name: "Test 3",
    permissions: ["<all_urls>"],
  });

  const ID4 = "addon4@tests.mozilla.org";
  await createWebExtension({
    id: ID4,
    name: "Test 4",
    permissions: ["<all_urls>"],
  });

  await promiseStartupManager();

  let sideloaded = await AddonManagerPrivate.getNewSideloads();

  sideloaded.sort((a, b) => a.id.localeCompare(b.id));

  deepEqual(sideloaded.map(a => a.id),
            [ID1, ID2, ID3, ID4],
            "Got the correct sideload add-ons");

  deepEqual(sideloaded.map(a => a.userDisabled),
            [true, true, true, true],
            "All sideloaded add-ons are disabled");
});
