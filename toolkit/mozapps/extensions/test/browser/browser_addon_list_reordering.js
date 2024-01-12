/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

function assertInSection(card, sectionName, msg) {
  let section = card.closest("section");
  let heading = section.querySelector(".list-section-heading");
  is(
    card.ownerDocument.l10n.getAttributes(heading).id,
    `extension-${sectionName}-heading`,
    msg
  );
}

function waitForAnimationFrame(win) {
  return new Promise(resolve => win.requestAnimationFrame(resolve));
}

async function clickEnableToggle(card) {
  let isDisabled = card.addon.userDisabled;
  let addonEvent = isDisabled ? "onEnabled" : "onDisabled";
  let addonStateChanged = AddonTestUtils.promiseAddonEvent(addonEvent);
  let win = card.ownerGlobal;
  let button = card.querySelector(".extension-enable-button");

  // Centre the button since "start" could be behind the sticky header.
  button.scrollIntoView({ block: "center" });
  EventUtils.synthesizeMouseAtCenter(button, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtCenter(button, {}, win);

  await addonStateChanged;
  await waitForAnimationFrame(win);
}

function mouseOver(el) {
  let win = el.ownerGlobal;
  el.scrollIntoView({ block: "center" });
  EventUtils.synthesizeMouseAtCenter(el, { type: "mousemove" }, win);
  return waitForAnimationFrame(win);
}

function mouseOutOfList(win) {
  return mouseOver(win.document.querySelector(".header-name"));
}

function pressKey(win, key) {
  EventUtils.synthesizeKey(key, {}, win);
  return waitForAnimationFrame(win);
}

function waitForTransitionEnd(...els) {
  return Promise.all(
    els.map(el =>
      BrowserTestUtils.waitForEvent(el, "transitionend", false, e => {
        let cardEl = el.firstElementChild;
        return e.target == cardEl && e.propertyName == "transform";
      })
    )
  );
}

add_setup(async function () {
  // Ensure prefers-reduced-motion isn't set. Some linux environments will have
  // this enabled by default.
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 0]],
  });
});

add_task(async function testReordering() {
  let addonIds = [
    "one@mochi.test",
    "two@mochi.test",
    "three@mochi.test",
    "four@mochi.test",
    "five@mochi.test",
  ];
  let extensions = addonIds.map(id =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: id,
        browser_specific_settings: { gecko: { id } },
      },
      useAddonManager: "temporary",
    })
  );

  await Promise.all(extensions.map(ext => ext.startup()));

  let win = await loadInitialView("extension", { withAnimations: true });

  let cardOne = getAddonCard(win, "one@mochi.test");
  ok(!cardOne.addon.userDisabled, "extension one is enabled");
  assertInSection(cardOne, "enabled", "cardOne is initially in Enabled");

  await clickEnableToggle(cardOne);

  ok(cardOne.addon.userDisabled, "extension one is now disabled");
  assertInSection(cardOne, "enabled", "cardOne is still in Enabled");

  let cardThree = getAddonCard(win, "three@mochi.test");
  ok(!cardThree.addon.userDisabled, "extension three is enabled");
  assertInSection(cardThree, "enabled", "cardThree is initially in Enabled");

  await clickEnableToggle(cardThree);

  ok(cardThree.addon.userDisabled, "extension three is now disabled");
  assertInSection(cardThree, "enabled", "cardThree is still in Enabled");

  let transitionsEnded = waitForTransitionEnd(cardOne, cardThree);
  await mouseOutOfList(win);
  await transitionsEnded;

  assertInSection(cardOne, "disabled", "cardOne has moved to disabled");
  assertInSection(cardThree, "disabled", "cardThree has moved to disabled");

  await clickEnableToggle(cardThree);
  await clickEnableToggle(cardOne);

  assertInSection(cardOne, "disabled", "cardOne is still in disabled");
  assertInSection(cardThree, "disabled", "cardThree is still in disabled");

  info("Opening a more options menu");
  let panel = cardThree.querySelector("panel-list");
  EventUtils.synthesizeMouseAtCenter(
    cardThree.querySelector('[action="more-options"]'),
    {},
    win
  );

  await BrowserTestUtils.waitForEvent(panel, "shown");
  await mouseOutOfList(win);

  assertInSection(cardOne, "disabled", "cardOne stays in disabled, menu open");
  assertInSection(cardThree, "disabled", "cardThree stays in disabled");

  transitionsEnded = waitForTransitionEnd(cardOne, cardThree);
  // We intentionally turn off this a11y check, because the following click
  // is purposefully targeting a non-interactive element to clear the focused
  // state with a mouse which can be done by assistive technology and keyboard
  // by pressing `Esc` key, this rule check shall be ignored by a11y_checks suite.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  // Click outside the list to clear any focus.
  EventUtils.synthesizeMouseAtCenter(
    win.document.querySelector(".header-name"),
    {},
    win
  );
  AccessibilityUtils.resetEnv();
  await transitionsEnded;

  assertInSection(cardOne, "enabled", "cardOne is now in enabled");
  assertInSection(cardThree, "enabled", "cardThree is now in enabled");

  let cardOneToggle = cardOne.querySelector(".extension-enable-button");
  cardOneToggle.scrollIntoView({ block: "center" });
  cardOneToggle.focus();
  await pressKey(win, " ");
  await waitForAnimationFrame(win);

  let cardThreeToggle = cardThree.querySelector(".extension-enable-button");
  let addonList = win.document.querySelector("addon-list");
  // Tab down to cardThreeToggle.
  while (
    addonList.contains(win.document.activeElement) &&
    win.document.activeElement !== cardThreeToggle
  ) {
    await pressKey(win, "VK_TAB");
  }
  await pressKey(win, " ");

  assertInSection(cardOne, "enabled", "cardOne is still in enabled");
  assertInSection(cardThree, "enabled", "cardThree is still in enabled");

  transitionsEnded = waitForTransitionEnd(cardOne, cardThree);
  win.document.querySelector('[action="page-options"]').focus();
  await transitionsEnded;
  assertInSection(
    cardOne,
    "disabled",
    "cardOne is now in the disabled section"
  );
  assertInSection(
    cardThree,
    "disabled",
    "cardThree is now in the disabled section"
  );

  // Ensure an uninstalled extension is removed right away.
  // Hover a card in the middle of the list.
  await mouseOver(getAddonCard(win, "two@mochi.test"));
  await cardOne.addon.uninstall(true);
  ok(!cardOne.parentNode, "cardOne has been removed from the document");

  await closeView(win);
  await Promise.all(extensions.map(ext => ext.unload()));
});
