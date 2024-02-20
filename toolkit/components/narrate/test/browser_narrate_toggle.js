/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that the keyboard shortcut "n" will Start/Stop the
// narration of an article in readermode when the article is in focus.
// This test also verifies that the keyboard shortcut "←" (left arrow) will
// skip the narration backward, while "→" (right arrow) skips it forward.

registerCleanupFunction(teardown);

add_task(async function testToggleNarrate() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function () {
    let TEST_VOICE = "urn:moz-tts:fake:teresa";
    let $ = content.document.querySelector.bind(content.document);

    let prefChanged = NarrateTestUtils.waitForPrefChange("narrate.voice");
    NarrateTestUtils.selectVoice(content, TEST_VOICE);
    await prefChanged;

    await NarrateTestUtils.waitForNarrateToggle(content);

    let eventUtils = NarrateTestUtils.getEventUtils(content);

    NarrateTestUtils.isStoppedState(content, ok);

    let promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.TOGGLE).focus();
    eventUtils.synthesizeKey("n", {}, content);
    let speechinfo = (await promiseEvent).detail;
    let paragraph = speechinfo.paragraph;

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    eventUtils.synthesizeKey("KEY_ArrowRight", {}, content);
    speechinfo = (await promiseEvent).detail;
    isnot(speechinfo.paragraph, paragraph, "next paragraph is being spoken");

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    eventUtils.synthesizeKey("KEY_ArrowLeft", {}, content);
    speechinfo = (await promiseEvent).detail;
    is(speechinfo.paragraph, paragraph, "first paragraph being spoken");

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
