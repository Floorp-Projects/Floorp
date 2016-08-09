/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals is, isnot, registerCleanupFunction, add_task */

"use strict";

registerCleanupFunction(teardown);

add_task(function* testNarrate() {
  setup();

  yield spawnInNewReaderTab(TEST_ARTICLE, function* () {
    let TEST_VOICE = "urn:moz-tts:fake-indirect:teresa";
    let $ = content.document.querySelector.bind(content.document);

    let popup = $(NarrateTestUtils.POPUP);
    ok(!NarrateTestUtils.isVisible(popup), "popup is initially hidden");

    let toggle = $(NarrateTestUtils.TOGGLE);
    toggle.click();

    ok(NarrateTestUtils.isVisible(popup), "popup toggled");

    yield NarrateTestUtils.waitForVoiceOptions(content);

    let voiceOptions = $(NarrateTestUtils.VOICE_OPTIONS);
    ok(!NarrateTestUtils.isVisible(voiceOptions),
      "voice options are initially hidden");

    $(NarrateTestUtils.VOICE_SELECT).click();
    ok(NarrateTestUtils.isVisible(voiceOptions), "voice options pop up");

    let prefChanged = NarrateTestUtils.waitForPrefChange("narrate.voice");
    ok(NarrateTestUtils.selectVoice(content, TEST_VOICE),
      "test voice selected");
    yield prefChanged;

    ok(!NarrateTestUtils.isVisible(voiceOptions), "voice options hidden again");

    NarrateTestUtils.isStoppedState(content, ok);

    let promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    let speechinfo = (yield promiseEvent).detail;
    is(speechinfo.voice, TEST_VOICE, "correct voice is being used");
    let paragraph = speechinfo.paragraph;

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.FORWARD).click();
    speechinfo = (yield promiseEvent).detail;
    is(speechinfo.voice, TEST_VOICE, "same voice is used");
    isnot(speechinfo.paragraph, paragraph, "next paragraph is being spoken");

    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.BACK).click();
    speechinfo = (yield promiseEvent).detail;
    is(speechinfo.paragraph, paragraph, "first paragraph being spoken");

    NarrateTestUtils.isStartedState(content, ok);

    let eventUtils = NarrateTestUtils.getEventUtils(content);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    prefChanged = NarrateTestUtils.waitForPrefChange("narrate.rate");
    $(NarrateTestUtils.RATE).focus();
    eventUtils.sendKey("PAGE_UP", content);
    let newspeechinfo = (yield promiseEvent).detail;
    is(newspeechinfo.paragraph, speechinfo.paragraph, "same paragraph");
    isnot(newspeechinfo.rate, speechinfo.rate, "rate changed");
    yield prefChanged;

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphend");
    $(NarrateTestUtils.STOP).click();
    yield promiseEvent;

    yield ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP), "transitioned to stopped state");
    NarrateTestUtils.isStoppedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "scroll");
    content.scrollBy(0, 10);
    yield promiseEvent;
    ok(!NarrateTestUtils.isVisible(popup), "popup is hidden after scroll");

    toggle.click();
    ok(NarrateTestUtils.isVisible(popup), "popup is toggled again");

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    yield promiseEvent;
    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "scroll");
    content.scrollBy(0, -10);
    yield promiseEvent;
    ok(NarrateTestUtils.isVisible(popup), "popup stays visible after scroll");

    toggle.click();
    ok(!NarrateTestUtils.isVisible(popup), "popup is dismissed while speaking");
    NarrateTestUtils.isStartedState(content, ok);

    promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphend");
    $(NarrateTestUtils.STOP).click();
    yield promiseEvent;
    yield ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP), "transitioned to stopped state");
    NarrateTestUtils.isStoppedState(content, ok);
  });
});
