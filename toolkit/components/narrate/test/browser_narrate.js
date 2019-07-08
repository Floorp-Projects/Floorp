/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(teardown);

add_task(async function testNarrate() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let TEST_VOICE = "urn:moz-tts:fake:teresa";
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    let popup = $(NarrateTestUtils.POPUP);
    ok(!NarrateTestUtils.isVisible(popup), "popup is initially hidden");

    let toggle = $(NarrateTestUtils.TOGGLE);
    toggle.click();

    ok(NarrateTestUtils.isVisible(popup), "popup toggled");

    let voiceOptions = $(NarrateTestUtils.VOICE_OPTIONS);
    ok(
      !NarrateTestUtils.isVisible(voiceOptions),
      "voice options are initially hidden"
    );

    $(NarrateTestUtils.VOICE_SELECT).click();
    ok(NarrateTestUtils.isVisible(voiceOptions), "voice options pop up");

    let prefChanged = NarrateTestUtils.waitForPrefChange("narrate.voice");
    ok(
      NarrateTestUtils.selectVoice(content, TEST_VOICE),
      "test voice selected"
    );
    await prefChanged;

    ok(!NarrateTestUtils.isVisible(voiceOptions), "voice options hidden again");

    NarrateTestUtils.isStoppedState(content, ok);

    let promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    let speechinfo = (await promiseEvent).detail;
    is(speechinfo.voice, TEST_VOICE, "correct voice is being used");
    let paragraph = speechinfo.paragraph;

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.FORWARD).click();
    speechinfo = (await promiseEvent).detail;
    is(speechinfo.voice, TEST_VOICE, "same voice is used");
    isnot(speechinfo.paragraph, paragraph, "next paragraph is being spoken");

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.BACK).click();
    speechinfo = (await promiseEvent).detail;
    is(speechinfo.paragraph, paragraph, "first paragraph being spoken");

    NarrateTestUtils.isStartedState(content, ok);

    paragraph = speechinfo.paragraph;
    $(NarrateTestUtils.STOP).click();
    await ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP),
      "transitioned to stopped state"
    );
    NarrateTestUtils.isStoppedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    speechinfo = (await promiseEvent).detail;
    is(speechinfo.paragraph, paragraph, "read same paragraph again");

    NarrateTestUtils.isStartedState(content, ok);

    let eventUtils = NarrateTestUtils.getEventUtils(content);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    prefChanged = NarrateTestUtils.waitForPrefChange("narrate.rate");
    $(NarrateTestUtils.RATE).focus();
    eventUtils.sendKey("UP", content);
    let newspeechinfo = (await promiseEvent).detail;
    is(newspeechinfo.paragraph, speechinfo.paragraph, "same paragraph");
    isnot(newspeechinfo.rate, speechinfo.rate, "rate changed");
    await prefChanged;

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphend");
    $(NarrateTestUtils.STOP).click();
    await promiseEvent;

    await ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP),
      "transitioned to stopped state"
    );
    NarrateTestUtils.isStoppedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "scroll");
    content.scrollBy(0, 10);
    await promiseEvent;
    ok(!NarrateTestUtils.isVisible(popup), "popup is hidden after scroll");

    toggle.click();
    ok(NarrateTestUtils.isVisible(popup), "popup is toggled again");

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    await promiseEvent;
    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "scroll");
    content.scrollBy(0, -10);
    await promiseEvent;
    ok(NarrateTestUtils.isVisible(popup), "popup stays visible after scroll");

    toggle.click();
    ok(!NarrateTestUtils.isVisible(popup), "popup is dismissed while speaking");
    NarrateTestUtils.isStartedState(content, ok);

    // Go forward all the way to the end of the article. We should eventually
    // stop.
    do {
      promiseEvent = Promise.race([
        ContentTaskUtils.waitForEvent(content, "paragraphstart"),
        ContentTaskUtils.waitForEvent(content, "paragraphsdone"),
      ]);
      $(NarrateTestUtils.FORWARD).click();
    } while ((await promiseEvent).type == "paragraphstart");

    // This is to make sure we are not actively scrolling when the tab closes.
    content.scroll(0, 0);

    await ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP),
      "transitioned to stopped state"
    );
    NarrateTestUtils.isStoppedState(content, ok);
  });
});
