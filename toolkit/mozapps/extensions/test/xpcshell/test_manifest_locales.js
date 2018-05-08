/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const ID = "bug397778@tests.mozilla.org";

const ADDON = {
  id: "bug397778@tests.mozilla.org",
  version: "1.0",
  name: "Fallback Name",
  description: "Fallback Description",
  bootstrap: true,

  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"}],

  localized: [
    {
      locale: ["fr"],
      name: "fr Name",
      description: "fr Description",
    },
    {
      locale: ["de-DE"],
      name: "Deutsches W\u00f6rterbuch",
    },
    {
      locale: ["es-ES"],
      name: "es-ES Name",
      description: "es-ES Description",
    },
    {
      locale: ["zh-TW"],
      name: "zh-TW Name",
      description: "zh-TW Description",
    },
    {
      locale: ["zh-CN"],
      name: "zh-CN Name",
      description: "zh-CN Description",
    },
    {
      locale: ["en-GB"],
      name: "en-GB Name",
      description: "en-GB Description",
    },
    {
      locale: ["en"],
      name: "en Name",
      description: "en Description",
    },
    {
      locale: ["en-CA"],
      name: "en-CA Name",
      description: "en-CA Description",
    },
  ],
};

function changeLocales(locales) {
  Services.locale.setRequestedLocales(locales);
  return promiseRestartManager();
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  Services.locale.setRequestedLocales(["fr-FR"]);

  await promiseStartupManager();
  await promiseInstallXPI(ADDON);
});

add_task(async function test_1() {
  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "fr Name");
  Assert.equal(addon.description, "fr Description");

  // Disable item
  addon.userDisabled = true;
  await promiseRestartManager();

  let newAddon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(newAddon, null);
  Assert.equal(newAddon.name, "fr Name");
});

add_task(async function test_2() {
  // Change locale. The more specific de-DE is the best match
  await changeLocales(["de"]);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "Deutsches W\u00f6rterbuch");
  Assert.equal(addon.description, null);
});

add_task(async function test_3() {
  // Change locale. Locale case should have no effect
  await changeLocales(["DE-de"]);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "Deutsches W\u00f6rterbuch");
  Assert.equal(addon.description, null);
});

add_task(async function test_4() {
  // Change locale. es-ES should closely match
  await changeLocales(["es-AR"]);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "es-ES Name");
  Assert.equal(addon.description, "es-ES Description");
});

add_task(async function test_5() {
  // Change locale. Either zh-CN or zh-TW could match
  await changeLocales(["zh"]);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  ok(addon.name == "zh-TW Name" || addon.name == "zh-CN Name",
     `Add-on name mismatch: ${addon.name}`);
});

add_task(async function test_6() {
  // Unknown locale should try to match against en-US as well. Of en,en-GB
  // en should match as being less specific
  await changeLocales(["nl-NL"]);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "en Name");
  Assert.equal(addon.description, "en Description");
});
