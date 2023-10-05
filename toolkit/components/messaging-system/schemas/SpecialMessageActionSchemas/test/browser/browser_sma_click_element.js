/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const {
  withFirefoxView,
  assertFirefoxViewTab,
  openFirefoxViewTab,
  closeFirefoxViewTab,
  init: FirefoxViewTestUtilsInit,
} = ChromeUtils.importESModule(
  "resource://testing-common/FirefoxViewTestUtils.sys.mjs"
);
FirefoxViewTestUtilsInit(this);

const TEST_MESSAGE = {
  message: {
    template: "feature_callout",
    content: {
      id: "TEST_MESSAGE",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      screens: [
        {
          id: "TEST_SCREEN_ID",
          anchors: [{ selector: "#tabpickup-steps", arrow_position: "top" }],
          content: {
            position: "callout",
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
                  selector: "span.brand-feature-name",
                },
              },
            },
          },
        },
      ],
    },
  },
};

let sandbox;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.firefox-view", true],
      ["browser.tabs.firefox-view-next", false],
    ],
  });

  sandbox = sinon.createSandbox();

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
});

add_task(async function test_CLICK_ELEMENT() {
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.resolves(TEST_MESSAGE);

  await withFirefoxView({ openNewWindow: true }, async browser => {
    const { document } = browser.contentWindow;
    const { FeatureCallout } = ChromeUtils.importESModule(
      "resource:///modules/FeatureCallout.sys.mjs"
    );
    let callout = new FeatureCallout({
      win: browser.contentWindow,
      location: "about:firefoxview",
      context: "content",
      theme: { preset: "themed-content" },
    });
    callout.showFeatureCallout();
    const calloutId = "feature-callout";
    const calloutSelector = `#${calloutId}.featureCallout`;

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
