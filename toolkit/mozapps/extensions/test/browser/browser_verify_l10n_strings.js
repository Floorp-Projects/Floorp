/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    ok(
      bundle.hasMessage(`extension-${l10nId}-name`),
      `l10n id for ${themeAddon.id} \"name\" attribute should exist`
    );
    ok(
      bundle.hasMessage(`extension-${l10nId}-description`),
      `l10n id for ${themeAddon.id} \"description\" attribute should exist`
    );
  }
});
