/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "webextension1@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");
profileDir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

async function testSimpleIconsetParsing(manifest) {
  await promiseWriteWebManifestForExtension(manifest, profileDir);

  await Promise.all([
    promiseRestartManager(),
    manifest.theme || promiseWebExtensionStartup(ID),
  ]);

  let uri = do_get_addon_root_uri(profileDir, ID);

  let addon = await promiseAddonByID(ID);
  do_check_neq(addon, null);

  function check_icons(addon_copy) {
    deepEqual(addon_copy.icons, {
        16: uri + "icon16.png",
        32: uri + "icon32.png",
        48: uri + "icon48.png",
        64: uri + "icon64.png"
    });

    // iconURL should map to icons[48] and icons[64]
    equal(addon.iconURL, uri + "icon48.png");
    equal(addon.icon64URL, uri + "icon64.png");

    // AddonManager gets the correct icon sizes from addon.icons
    equal(AddonManager.getPreferredIconURL(addon, 1), uri + "icon16.png");
    equal(AddonManager.getPreferredIconURL(addon, 16), uri + "icon16.png");
    equal(AddonManager.getPreferredIconURL(addon, 30), uri + "icon32.png");
    equal(AddonManager.getPreferredIconURL(addon, 48), uri + "icon48.png");
    equal(AddonManager.getPreferredIconURL(addon, 64), uri + "icon64.png");
    equal(AddonManager.getPreferredIconURL(addon, 128), uri + "icon64.png");
  }

  check_icons(addon);

  // check if icons are persisted through a restart
  await Promise.all([
    promiseRestartManager(),
    manifest.theme || promiseWebExtensionStartup(ID),
  ]);

  addon = await promiseAddonByID(ID);
  do_check_neq(addon, null);

  check_icons(addon);

  addon.uninstall();
}

async function testRetinaIconsetParsing(manifest) {
  await promiseWriteWebManifestForExtension(manifest, profileDir);

  await Promise.all([
    promiseRestartManager(),
    manifest.theme || promiseWebExtensionStartup(ID),
  ]);

  let addon = await promiseAddonByID(ID);
  do_check_neq(addon, null);

  let uri = do_get_addon_root_uri(profileDir, ID);

  // AddonManager displays larger icons for higher pixel density
  equal(AddonManager.getPreferredIconURL(addon, 32, {
    devicePixelRatio: 2
  }), uri + "icon64.png");

  equal(AddonManager.getPreferredIconURL(addon, 48, {
    devicePixelRatio: 2
  }), uri + "icon128.png");

  equal(AddonManager.getPreferredIconURL(addon, 64, {
    devicePixelRatio: 2
  }), uri + "icon128.png");

  addon.uninstall();
}

async function testNoIconsParsing(manifest) {
  await promiseWriteWebManifestForExtension(manifest, profileDir);

  await Promise.all([
    promiseRestartManager(),
    manifest.theme || promiseWebExtensionStartup(ID),
  ]);

  let addon = await promiseAddonByID(ID);
  do_check_neq(addon, null);

  deepEqual(addon.icons, {});

  equal(addon.iconURL, null);
  equal(addon.icon64URL, null);

  equal(AddonManager.getPreferredIconURL(addon, 128), null);

  addon.uninstall();
}

// Test simple icon set parsing
add_task(async function() {
  await testSimpleIconsetParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    },
    icons: {
      16: "icon16.png",
      32: "icon32.png",
      48: "icon48.png",
      64: "icon64.png"
    }
  });

  // Now for theme-type extensions too.
  await testSimpleIconsetParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    },
    icons: {
      16: "icon16.png",
      32: "icon32.png",
      48: "icon48.png",
      64: "icon64.png"
    },
    theme: { images: { headerURL: "example.png" } }
  });
});

// Test AddonManager.getPreferredIconURL for retina screen sizes
add_task(async function() {
  await testRetinaIconsetParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    },
    icons: {
      32: "icon32.png",
      48: "icon48.png",
      64: "icon64.png",
      128: "icon128.png",
      256: "icon256.png"
    }
  });

  await testRetinaIconsetParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    },
    icons: {
      32: "icon32.png",
      48: "icon48.png",
      64: "icon64.png",
      128: "icon128.png",
      256: "icon256.png"
    },
    theme: { images: { headerURL: "example.png" } }
  });
});

// Handles no icons gracefully
add_task(async function() {
  await testNoIconsParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  });

  await testNoIconsParsing({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    },
    theme: { images: { headerURL: "example.png" } }
  });
});
