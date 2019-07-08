//
// This file provides helpers for tests of addons that use strictCompatibility.
// Since WebExtensions cannot opt out of strictCompatibility, we add a
// simple extension loader that lets tests directly set AddonInternal
// properties (including strictCompatibility)
//

/* import-globals-from head_addons.js */

const MANIFEST = "compat_manifest.json";

AddonManager.addExternalExtensionLoader({
  name: "compat-test",
  manifestFile: MANIFEST,
  async loadManifest(pkg) {
    // XPIDatabase.jsm gets unloaded in AddonTestUtils when the
    // addon manager is restarted.  Work around that by just importing
    // it every time we need to create an AddonInternal.
    const { AddonInternal } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIDatabase.jsm"
    );
    let addon = new AddonInternal();
    let manifest = JSON.parse(await pkg.readString(MANIFEST));
    Object.assign(addon, manifest);
    return addon;
  },
  loadScope(addon, file) {
    return {
      install() {},
      uninstall() {},
      startup() {},
      shutdonw() {},
    };
  },
});

const DEFAULTS = {
  defaultLocale: {},
  locales: [],
  targetPlatforms: [],
  type: "extension",
  version: "1.0",
};

function createAddon(manifest) {
  return AddonTestUtils.createTempXPIFile({
    [MANIFEST]: Object.assign({}, DEFAULTS, manifest),
  });
}
