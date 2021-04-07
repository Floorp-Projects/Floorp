/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  // Disable Proton for this test due to crashes seen when
  // attempting to enable Proton on autoland
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.enabled", false]],
  });
});

add_task(async function testPanelItemWithAccesskey() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let panelList = doc.createElement("panel-list");
  let items = [
    { textContent: "First Item", accessKey: "F" },
    { textContent: "Second Item", accessKey: "S" },
    { textContent: "Third Item" },
  ];

  let panelItems = items.map(details => {
    let item = doc.createElement("panel-item");
    for (let [attr, value] of Object.entries(details)) {
      item[attr] = value;
    }
    panelList.appendChild(item);
    return item;
  });

  doc.body.appendChild(panelList);

  function assertAccessKeys(items, keys, { checkLabels = false } = {}) {
    is(items.length, keys.length, "Got the same number of items and keys");
    for (let i = 0; i < items.length; i++) {
      is(items[i].accessKey, keys[i], `Item ${i} has the right key`);
      if (checkLabels) {
        let label = items[i].shadowRoot.querySelector("label");
        is(label.accessKey, keys[i] || null, `Label ${i} has the right key`);
      }
    }
  }

  info("Accesskeys should be removed when closed");
  assertAccessKeys(panelItems, ["", "", ""]);

  info("Accesskeys should be set when open");
  let panelShown = BrowserTestUtils.waitForEvent(panelList, "shown");
  panelList.show();
  await panelShown;
  assertAccessKeys(panelItems, ["F", "S", ""], { checkLabels: true });

  info("Changing accesskeys should happen right away");
  panelItems[1].accessKey = "c";
  panelItems[2].accessKey = "T";
  assertAccessKeys(panelItems, ["F", "c", "T"], { checkLabels: true });

  info("Accesskeys are removed again on hide");
  let panelHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
  panelList.hide();
  await panelHidden;
  assertAccessKeys(panelItems, ["", "", ""]);

  info("Accesskeys are set again on show");
  panelItems[0].removeAttribute("accesskey");
  panelShown = BrowserTestUtils.waitForEvent(panelList, "shown");
  panelList.show();
  await panelShown;
  assertAccessKeys(panelItems, ["", "c", "T"], { checkLabels: true });

  info("Check that accesskeys can be used without the modifier when open");
  let secondClickCount = 0;
  let thirdClickCount = 0;
  panelItems[1].addEventListener("click", () => secondClickCount++);
  panelItems[2].addEventListener("click", () => thirdClickCount++);

  // Make sure the focus is in the window.
  panelItems[0].focus();

  panelHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
  EventUtils.synthesizeKey("c", {}, win);
  await panelHidden;

  is(secondClickCount, 1, "The accesskey worked unmodified");
  is(thirdClickCount, 0, "The other listener wasn't fired");

  EventUtils.synthesizeKey("c", {}, win);
  EventUtils.synthesizeKey("t", {}, win);

  is(secondClickCount, 1, "The key doesn't trigger when closed");
  is(thirdClickCount, 0, "The key doesn't trigger when closed");

  panelShown = BrowserTestUtils.waitForEvent(panelList, "shown");
  panelList.show();
  await panelShown;

  panelHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
  EventUtils.synthesizeKey("t", {}, win);
  await panelHidden;

  is(secondClickCount, 1, "The other listener wasn't fired");
  is(thirdClickCount, 1, "The accesskey worked unmodified");

  await closeView(win);
});
