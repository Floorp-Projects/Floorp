/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Maps add-on descriptors to updated Fluent IDs. Keep it in sync
// with the list in XPIDatabase.jsm.
const updatedAddonFluentIds = new Map([
  ["extension-default-theme-name", "extension-default-theme-name-auto"],
]);

add_task(async function test_ensure_bundled_addons_are_localized() {
  let l10nReg = L10nRegistry.getInstance();
  let bundles = l10nReg.generateBundlesSync(
    ["en-US"],
    ["browser/appExtensionFields.ftl"]
  );
  let addons = await AddonManager.getAllAddons();
  let themeAddons = addons.filter(
    addon =>
      addon.isBuiltin &&
      // Temporary workaround until bug 1731652 lands.
      !addon.id.endsWith("colorway@mozilla.org") &&
      addon.type === "theme"
  );
  let bundle = bundles.next().value;

  ok(!!themeAddons.length, "Theme addons should exist");

  for (let themeAddon of themeAddons) {
    let l10nId = themeAddon.id.replace("@mozilla.org", "");
    for (let prop of ["name", "description"]) {
      let defaultFluentId = `extension-${l10nId}-${prop}`;
      let fluentId =
        updatedAddonFluentIds.get(defaultFluentId) || defaultFluentId;
      ok(
        bundle.hasMessage(fluentId),
        `l10n id for ${themeAddon.id} \"${prop}\" attribute should exist`
      );
    }
  }
});
