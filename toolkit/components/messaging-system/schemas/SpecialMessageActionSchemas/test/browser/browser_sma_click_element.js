/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const TEST_MESSAGE = {
  message: {
    content: {
      id: "TEST_MESSAGE",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      screens: [
        {
          id: "TEST_SCREEN_ID",
          parent_selector: "#tabpickup-steps",
          content: {
            position: "callout",
            arrow_position: "top",
            title: {
              string_id: "Test",
            },
            subtitle: {
              string_id: "Test",
            },
            primary_button: {
              label: {
                string_id: "Test",
              },
              action: {
                type: "CLICK_ELEMENT",
                data: {
                  selector:
                    "#tab-pickup-container button.primary:not(#error-state-button)",
                },
              },
            },
          },
        },
      ],
    },
  },
};

/**
 * Like in ./browser_sma_open_firefox_view.js,
 * the setup code and the utility funcitons here are cribbed
 * from (mostly) browser/components/firefoxview/test/browser/head.js
 *
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1784979 has been filed to move
 * these to some place publically accessible, after which we will be able to
 * a bunch of code from this file.
 */

let sandbox;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view", true]],
  });

  sandbox = sinon.createSandbox();

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
});

async function withFirefoxView({ win = null }, taskFn) {
  let shouldCloseWin = false;
  if (!win) {
    win = await BrowserTestUtils.openNewBrowserWindow();
    shouldCloseWin = true;
  }
  let tab = await openFirefoxViewTab(win);
  let originalWindow = tab.ownerGlobal;
  let result = await taskFn(tab.linkedBrowser);
  let finalWindow = tab.ownerGlobal;
  if (originalWindow == finalWindow && !tab.closing && tab.linkedBrowser) {
    // taskFn may resolve within a tick after opening a new tab.
    // We shouldn't remove the newly opened tab in the same tick.
    // Wait for the next tick here.
    await TestUtils.waitForTick();
    BrowserTestUtils.removeTab(tab);
  } else {
    Services.console.logStringMessage(
      "withFirefoxView: Tab was already closed before " +
        "removeTab would have been called"
    );
  }

  if (shouldCloseWin) {
    await BrowserTestUtils.closeWindow(win);
  }
  return result;
}

async function openFirefoxViewTab(w) {
  ok(
    !w.FirefoxViewHandler.tab,
    "Firefox View tab doesn't exist prior to clicking the button"
  );
  info("Clicking the Firefox View button");
  await EventUtils.synthesizeMouseAtCenter(
    w.document.getElementById("firefox-view-button"),
    { type: "mousedown" },
    w
  );
  assertFirefoxViewTab(w);
  ok(w.FirefoxViewHandler.tab.selected, "Firefox View tab is selected");
  await BrowserTestUtils.browserLoaded(w.FirefoxViewHandler.tab.linkedBrowser);
  return w.FirefoxViewHandler.tab;
}

function assertFirefoxViewTab(w) {
  ok(w.FirefoxViewHandler.tab, "Firefox View tab exists");
  ok(w.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  is(
    w.gBrowser.visibleTabs.indexOf(w.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
}

add_task(async function test_CLICK_ELEMENT() {
  SpecialPowers.pushPrefEnv([
    "browser.firefox-view.feature-tour",
    JSON.stringify({
      message: "",
      screen: "",
      complete: true,
    }),
  ]);

  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.resolves(TEST_MESSAGE);

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const calloutSelector = "#root.featureCallout";

    await BrowserTestUtils.waitForCondition(() => {
      return document.querySelector(
        `${calloutSelector}:not(.hidden) .${TEST_MESSAGE.message.content.screens[0].id}`
      );
    });

    // Clicking the CTA with the CLICK_ELEMENT action should result in the element found with the configured selector being clicked
    const clickElementSelector =
      TEST_MESSAGE.message.content.screens[0].content.primary_button.action.data
        .selector;
    const clickElement = document.querySelector(clickElementSelector);
    const successClick = () => {
      ok(true, "Configured element was clicked");
      clickElement.removeEventListener("click", successClick);
    };

    clickElement.addEventListener("click", successClick);
    document.querySelector(`${calloutSelector} button.primary`).click();
  });
});
