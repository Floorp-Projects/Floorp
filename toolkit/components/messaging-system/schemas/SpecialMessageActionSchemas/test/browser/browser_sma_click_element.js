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

add_task(async function test_CLICK_ELEMENT() {
  const calloutSelector = "#root.featureCallout";
  sinon.stub(ASRouter, "sendTriggerMessage").returns(TEST_MESSAGE);
  const clickStub = sinon.stub();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      await BrowserTestUtils.waitForCondition(() => {
        return document.querySelector(
          `${calloutSelector}:not(.hidden) .${TEST_MESSAGE.message.content.screens[0].id}`
        );
      });

      // Clicking the CTA with the CLICK_ELEMENT action should result in the element found with the configured selector being clicked
      const clickElementSelector =
        TEST_MESSAGE.message.content.screens[0].content.primary_button.action
          .data.selector;
      const clickElement = document.querySelector(clickElementSelector);
      clickElement.addEventListener("click", clickStub);
      document.querySelector(`${calloutSelector} button.primary`).click();

      Assert.equal(clickStub.calledOnce, true);
    }
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
