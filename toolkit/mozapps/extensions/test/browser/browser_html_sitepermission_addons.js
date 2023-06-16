/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { SITEPERMS_ADDON_PROVIDER_PREF, SITEPERMS_ADDON_TYPE } =
  ChromeUtils.importESModule(
    "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs"
  );

const html = `<!DOCTYPE html><h1>Test midi permission with synthetic site permission addon</h1>`;
const EXAMPLE_COM_URL = `https://example.com/document-builder.sjs?html=${html}`;
const EXAMPLE_ORG_URL = `https://example.org/document-builder.sjs?html=${html}`;

async function uninstallAllSitePermissionAddons() {
  const sitepermAddons = await AddonManager.getAddonsByTypes([
    SITEPERMS_ADDON_TYPE,
  ]);
  for (const addon of sitepermAddons) {
    await addon.uninstall();
  }
}

add_setup(async () => {
  registerCleanupFunction(uninstallAllSitePermissionAddons);
});

add_task(async function testAboutAddonUninstall() {
  if (!AddonManager.hasProvider("SitePermsAddonProvider")) {
    ok(
      !Services.prefs.getBoolPref(SITEPERMS_ADDON_PROVIDER_PREF),
      "Expect SitePermsAddonProvider to be disabled by prefs"
    );
    ok(true, "Skip test on SitePermsAddonProvider disabled");
    return;
  }

  // Grant midi permission on example.com so about:addons does have a Site Permissions section
  await SpecialPowers.addPermission("midi-sysex", true, EXAMPLE_COM_URL);

  info("Open an about:addon tab so AMO event listeners are registered");
  const aboutAddonWin = await loadInitialView("sitepermission");
  // loadInitialView sets the about:addon as the active one, so we can grab it here.
  const aboutAddonTab = gBrowser.selectedTab;

  const addonList = aboutAddonWin.document.querySelector("addon-list");
  let addonCards = aboutAddonWin.document.querySelectorAll("addon-card");
  is(
    addonCards.length,
    1,
    "There's a card displayed for the example.com addon"
  );

  info("Open an example.org tab and install the midi site permission addon");
  const exampleOrgTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    EXAMPLE_ORG_URL,
    true /* waitForLoad */
  );

  let promiseAddonCardAdded = BrowserTestUtils.waitForEvent(addonList, "add");

  info("Install midi");
  await testInstallGatedPermission(
    exampleOrgTab,
    () => {
      content.navigator.requestMIDIAccess();
    },
    "midi"
  );

  info("Install midi-sysex as well");
  const newAddon = await testInstallGatedPermission(
    exampleOrgTab,
    () => {
      content.navigator.requestMIDIAccess({ sysex: true });
    },
    "midi-sysex"
  );

  const newAddonId = newAddon.id;
  ok(
    newAddonId,
    "Got the addon id for the newly installed sitepermission add-on"
  );

  info("Switch back to about:addon");
  gBrowser.selectedTab = aboutAddonTab;

  await promiseAddonCardAdded;

  is(
    aboutAddonWin.document.querySelectorAll("addon-card").length,
    2,
    "A new addon card has been added as expected"
  );

  const exampleOrgAddonCard = getAddonCard(aboutAddonWin, newAddonId);

  info("Remove the example.org addon");
  const promptService = mockPromptService();
  promptService._response = 0;

  let promiseRemoved = BrowserTestUtils.waitForEvent(addonList, "remove");
  exampleOrgAddonCard.querySelector("[action=remove]").click();
  await promiseRemoved;

  is(
    aboutAddonWin.document.querySelectorAll("addon-card").length,
    1,
    "addon card has been removed as expected"
  );

  ok(
    await SpecialPowers.testPermission(
      "midi",
      SpecialPowers.Services.perms.UNKNOWN_ACTION,
      { url: EXAMPLE_ORG_URL }
    ),
    "midi permission was revoked"
  );
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.UNKNOWN_ACTION,
      { url: EXAMPLE_ORG_URL }
    ),
    "midi-sysex permission was revoked as well"
  );

  await BrowserTestUtils.removeTab(exampleOrgTab);
  await close_manager(aboutAddonWin);
  await uninstallAllSitePermissionAddons();
});

/**
 *
 * Execute a function in the tab content page and check that the expected gated permission
 * is set
 *
 * @param {Tab} tab: The tab in which we want to install the gated permission
 * @param {Function} spawnCallback: function used in `SpecialPowers.spawn` that will trigger
 *                                  the install
 * @param {String} expectedPermType: The name of the permission that should be granted
 * @returns {Promise<Addon>} The installed addon instance
 */
async function testInstallGatedPermission(
  tab,
  spawnCallback,
  expectedPermType
) {
  let onInstallEnded = AddonTestUtils.promiseInstallEvent("onInstallEnded");
  let onAddonInstallBlockedNotification = promisePopupNotificationShown(
    "addon-install-blocked"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], spawnCallback);

  let addonInstallPanel = await onAddonInstallBlockedNotification;
  let dialogPromise = promisePopupNotificationShown("addon-webext-permissions");
  addonInstallPanel.button.click();
  let installPermsDialog = await dialogPromise;
  installPermsDialog.button.click();

  const addon = await onInstallEnded.then(install => install[0].addon);
  // Close the addon-installed dialog to avoid interfering with other tests
  await acceptAppMenuNotificationWhenShown("addon-installed", addon.id);

  ok(
    await SpecialPowers.testPermission(
      expectedPermType,
      SpecialPowers.Services.perms.ALLOW_ACTION,
      { url: EXAMPLE_ORG_URL }
    ),
    `"${expectedPermType}" permission was granted`
  );
  return addon;
}
