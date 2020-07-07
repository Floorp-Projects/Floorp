/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);

add_task(async function test_all_test_messages() {
  let messagesWithButtons = CFRMessageProvider.getMessages().filter(
    m => m.content.buttons
  );

  for (let message of messagesWithButtons) {
    info(`Testing ${message.id}`);
    let { primary, secondary } = message.content.buttons;
    await SMATestUtils.validateAction(primary.action);
    for (let secondaryBtn of secondary) {
      if (secondaryBtn.action) {
        await SMATestUtils.validateAction(secondaryBtn.action);
      }
    }
  }
});
