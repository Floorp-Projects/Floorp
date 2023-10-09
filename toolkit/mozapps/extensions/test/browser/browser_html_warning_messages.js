/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

let gProvider;
const { STATE_BLOCKED, STATE_SOFTBLOCKED } = Ci.nsIBlocklistService;

const appVersion = Services.appinfo.version;
const SUPPORT_URL = Services.urlFormatter.formatURL(
  Services.prefs.getStringPref("app.support.baseURL")
);

add_setup(async function () {
  gProvider = new MockProvider();
});

async function checkMessageState(id, addonType, expected) {
  async function checkAddonCard() {
    let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
    let messageBar = card.querySelector(".addon-card-message");

    if (!expected) {
      ok(messageBar.hidden, "message is hidden");
    } else {
      const { linkUrl, text, type } = expected;

      await BrowserTestUtils.waitForMutationCondition(
        messageBar,
        { attributes: true },
        () => !messageBar.hidden
      );
      ok(!messageBar.hidden, "message is visible");

      is(messageBar.getAttribute("type"), type, "message has the right type");
      Assert.deepEqual(
        document.l10n.getAttributes(messageBar),
        { id: `${text.id}2`, args: text.args },
        "message l10n data is set correctly"
      );

      const link = messageBar.querySelector("button");
      if (linkUrl) {
        ok(!link.hidden, "link is visible");
        is(
          link.getAttribute("data-l10n-id"),
          `${text.id}-link`,
          "link l10n id is correct"
        );
        const newTab = BrowserTestUtils.waitForNewTab(gBrowser, linkUrl);
        link.click();
        BrowserTestUtils.removeTab(await newTab);
      } else {
        ok(link.hidden, "link is hidden");
      }
    }

    return card;
  }

  let win = await loadInitialView(addonType);
  let doc = win.document;

  // Check the list view.
  ok(doc.querySelector("addon-list"), "this is a list view");
  let card = await checkAddonCard();

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Check the detail view.
  ok(!doc.querySelector("addon-list"), "this isn't a list view");
  await checkAddonCard();

  await closeView(win);
}

add_task(async function testNoMessageExtension() {
  let id = "no-message@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { browser_specific_settings: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  await checkMessageState(id, "extension", null);

  await extension.unload();
});

add_task(async function testNoMessageLangpack() {
  let id = "no-message@mochi.test";
  gProvider.createAddons([
    {
      appDisabled: true,
      id,
      name: "Signed Langpack",
      signedState: AddonManager.SIGNEDSTATE_SIGNED,
      type: "locale",
    },
  ]);

  await checkMessageState(id, "locale", null);
});

add_task(async function testBlocked() {
  const id = "blocked@mochi.test";
  const linkUrl = "https://example.com/addon-blocked";
  const name = "Blocked";
  gProvider.createAddons([
    {
      appDisabled: true,
      blocklistState: STATE_BLOCKED,
      blocklistURL: linkUrl,
      id,
      isActive: false,
      name,
    },
  ]);
  await checkMessageState(id, "extension", {
    linkUrl,
    text: { id: "details-notification-blocked", args: { name } },
    type: "error",
  });
});

add_task(async function testUnsignedDisabled() {
  // This pref being disabled will cause the `specialpowers` addon to be
  // uninstalled, which can cause a number of test failures due to features no
  // longer working correctly.
  // In order to avoid those issues, this code manually disables the pref, and
  // ensures that `SpecialPowers` is fully re-enabled at the end of the test.
  const sigPref = "xpinstall.signatures.required";
  Services.prefs.setBoolPref(sigPref, true);

  const id = "unsigned@mochi.test";
  const name = "Unsigned";
  gProvider.createAddons([
    {
      appDisabled: true,
      id,
      name,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
    },
  ]);
  await checkMessageState(id, "extension", {
    linkUrl: SUPPORT_URL + "unsigned-addons",
    text: { id: "details-notification-unsigned-and-disabled", args: { name } },
    type: "error",
  });

  // Ensure that `SpecialPowers` is fully re-initialized at the end of this
  // test. This requires removing the existing binding so that it's
  // re-registered, re-enabling unsigned extensions, and then waiting for the
  // actor to be registered and ready.
  delete window.SpecialPowers;
  Services.prefs.setBoolPref(sigPref, false);
  await TestUtils.waitForCondition(() => {
    try {
      return !!windowGlobalChild.getActor("SpecialPowers");
    } catch (e) {
      return false;
    }
  }, "wait for SpecialPowers to be reloaded");
  ok(window.SpecialPowers, "SpecialPowers should be re-defined");
});

add_task(async function testUnsignedLangpackDisabled() {
  const id = "unsigned-langpack@mochi.test";
  const name = "Unsigned";
  gProvider.createAddons([
    {
      appDisabled: true,
      id,
      name,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
      type: "locale",
    },
  ]);
  await checkMessageState(id, "locale", {
    linkUrl: SUPPORT_URL + "unsigned-addons",
    text: { id: "details-notification-unsigned-and-disabled", args: { name } },
    type: "error",
  });
});

add_task(async function testIncompatible() {
  const id = "incompatible@mochi.test";
  const name = "Incompatible";
  gProvider.createAddons([
    {
      appDisabled: true,
      id,
      isActive: false,
      isCompatible: false,
      name,
    },
  ]);
  await checkMessageState(id, "extension", {
    text: {
      id: "details-notification-incompatible",
      args: { name, version: appVersion },
    },
    type: "error",
  });
});

add_task(async function testUnsignedEnabled() {
  const id = "unsigned-allowed@mochi.test";
  const name = "Unsigned";
  gProvider.createAddons([
    {
      id,
      name,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
    },
  ]);
  await checkMessageState(id, "extension", {
    linkUrl: SUPPORT_URL + "unsigned-addons",
    text: { id: "details-notification-unsigned", args: { name } },
    type: "warning",
  });
});

add_task(async function testUnsignedLangpackEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.langpacks.signatures.required", false]],
  });

  const id = "unsigned-allowed-langpack@mochi.test";
  const name = "Unsigned Langpack";
  gProvider.createAddons([
    {
      id,
      name,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
      type: "locale",
    },
  ]);
  await checkMessageState(id, "locale", {
    linkUrl: SUPPORT_URL + "unsigned-addons",
    text: { id: "details-notification-unsigned", args: { name } },
    type: "warning",
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function testSoftBlocked() {
  const id = "softblocked@mochi.test";
  const linkUrl = "https://example.com/addon-blocked";
  const name = "Soft Blocked";
  gProvider.createAddons([
    {
      appDisabled: true,
      blocklistState: STATE_SOFTBLOCKED,
      blocklistURL: linkUrl,
      id,
      isActive: false,
      name,
    },
  ]);
  await checkMessageState(id, "extension", {
    linkUrl,
    text: { id: "details-notification-softblocked", args: { name } },
    type: "warning",
  });
});

add_task(async function testPluginInstalling() {
  const id = "plugin-installing@mochi.test";
  const name = "Plugin Installing";
  gProvider.createAddons([
    {
      id,
      isActive: true,
      isGMPlugin: true,
      isInstalled: false,
      name,
      type: "plugin",
    },
  ]);
  await checkMessageState(id, "plugin", {
    text: { id: "details-notification-gmp-pending", args: { name } },
    type: "warning",
  });
});
