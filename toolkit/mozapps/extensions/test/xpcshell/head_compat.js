//
// This file provides helpers for tests of addons that use strictCompatibility.
// Since WebExtensions cannot opt out of strictCompatibility, we add a
// simple extension loader that lets tests directly set AddonInternal
// properties (including strictCompatibility)
//

/* import-globals-from head_addons.js */

const { XPIExports } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/XPIExports.sys.mjs"
);

const MANIFEST = "compat_manifest.json";

AddonManager.addExternalExtensionLoader({
  name: "compat-test",
  manifestFile: MANIFEST,
  async loadManifest(pkg) {
    let addon = new XPIExports.AddonInternal();
    let manifest = JSON.parse(await pkg.readString(MANIFEST));
    Object.assign(addon, manifest);
    return addon;
  },
  loadScope() {
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
