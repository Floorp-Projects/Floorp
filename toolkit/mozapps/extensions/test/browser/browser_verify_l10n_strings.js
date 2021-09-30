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
  let standardBuiltInThemes = addons.filter(
    addon =>
      addon.isBuiltin &&
      addon.type === "theme" &&
      !addon.id.endsWith("colorway@mozilla.org")
  );
  let bundle = bundles.next().value;

  ok(!!standardBuiltInThemes.length, "Standard built-in themes should exist");

  for (let standardTheme of standardBuiltInThemes) {
    let l10nId = standardTheme.id.replace("@mozilla.org", "");
    for (let prop of ["name", "description"]) {
      let defaultFluentId = `extension-${l10nId}-${prop}`;
      let fluentId =
        updatedAddonFluentIds.get(defaultFluentId) || defaultFluentId;
      ok(
        bundle.hasMessage(fluentId),
        `l10n id for ${standardTheme.id} \"${prop}\" attribute should exist`
      );
    }
  }

  // We verify colorway themes separately so we can more easily remove or modify
  // this section when they are removed.
  let colorwayBuiltInThemes = addons.filter(
    addon =>
      addon.isBuiltin &&
      addon.type === "theme" &&
      addon.id.endsWith("colorway@mozilla.org")
  );
  ok(!!colorwayBuiltInThemes.length, "Colorway themes should exist");
  for (let colorwayTheme of colorwayBuiltInThemes) {
    let l10nId = colorwayTheme.id.replace("@mozilla.org", "");
    let [, variantName] = l10nId.split("-", 2);
    let defaultFluentId = `extension-colorways-${variantName}-name`;
    let fluentId =
      updatedAddonFluentIds.get(defaultFluentId) || defaultFluentId;
    ok(
      bundle.hasMessage(fluentId),
      `l10n id for ${colorwayTheme.id} \"name\" attribute should exist`
    );
  }
});
