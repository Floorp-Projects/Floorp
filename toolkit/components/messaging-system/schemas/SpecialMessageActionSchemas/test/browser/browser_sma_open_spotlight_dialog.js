/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);

const { Spotlight } = ChromeUtils.import(
  "resource://activity-stream/lib/Spotlight.jsm"
);

add_task(async function test_OPEN_SPOTLIGHT_DIALOG() {
  let pbNewTabMessage = (
    await OnboardingMessageProvider.getUntranslatedMessages()
  ).filter(m => m.id === "PB_NEWTAB_FOCUS_PROMO");
  info(`Testing ${pbNewTabMessage[0].id}`);
  let showSpotlightStub = sinon.stub(Spotlight, "showSpotlightDialog");
  await SMATestUtils.executeAndValidateAction({
    type: "SHOW_SPOTLIGHT",
    data: { ...pbNewTabMessage[0].content.promoButton.action.data },
  });

  Assert.equal(
    showSpotlightStub.callCount,
    1,
    "Should call showSpotlightDialog"
  );

  Assert.deepEqual(
    showSpotlightStub.firstCall.args[1],
    pbNewTabMessage[0].content.promoButton.action.data,
    "Should be called with action.data"
  );

  showSpotlightStub.restore();
});
