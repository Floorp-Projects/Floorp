/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that the keyboard shortcut "n" will Start/Stop the
// narration of an article in readermode when the article is in focus.

registerCleanupFunction(teardown);

add_task(async function testToggleNarrate() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function () {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    let eventUtils = NarrateTestUtils.getEventUtils(content);

    NarrateTestUtils.isStoppedState(content, ok);

    $(NarrateTestUtils.TOGGLE).focus();
    eventUtils.synthesizeKey("n", {}, content);

    await ContentTaskUtils.waitForEvent(content, "paragraphstart");
    NarrateTestUtils.isStartedState(content, ok);

    $(NarrateTestUtils.TOGGLE).focus();
    eventUtils.synthesizeKey("n", {}, content);

    await ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP),
      "transitioned to stopped state"
    );
    NarrateTestUtils.isStoppedState(content, ok);
  });
});
