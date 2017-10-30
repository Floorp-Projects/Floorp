/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable mozilla/no-cpows-in-tests */

registerCleanupFunction(teardown);

add_task(async function testVoiceselectDropdownAutoclose() {
  setup("automatic", true);

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    ok(!!$(".option[data-value='urn:moz-tts:fake-direct:bob']"),
      "Jamaican English voice available");
    ok(!!$(".option[data-value='urn:moz-tts:fake-direct:lenny']"),
      "Canadian English voice available");
    ok(!!$(".option[data-value='urn:moz-tts:fake-direct:amy']"),
      "British English voice available");

    ok(!$(".option[data-value='urn:moz-tts:fake-direct:celine']"),
      "Canadian French voice unavailable");
    ok(!$(".option[data-value='urn:moz-tts:fake-direct:julie']"),
      "Mexican Spanish voice unavailable");

    $(NarrateTestUtils.TOGGLE).click();
    ok(NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled");

    let prefChanged = NarrateTestUtils.waitForPrefChange(
      "narrate.voice", "getCharPref");
    NarrateTestUtils.selectVoice(content, "urn:moz-tts:fake-direct:lenny");
    let voicePref = JSON.parse(await prefChanged);
    is(voicePref.en, "urn:moz-tts:fake-direct:lenny", "pref set correctly");
  });
});

add_task(async function testVoiceselectDropdownAutoclose() {
  setup("automatic", true);

  await spawnInNewReaderTab(TEST_ITALIAN_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    ok(!!$(".option[data-value='urn:moz-tts:fake-indirect:zanetta']"),
      "Italian voice available");
    ok(!!$(".option[data-value='urn:moz-tts:fake-indirect:margherita']"),
      "Italian voice available");

    ok(!$(".option[data-value='urn:moz-tts:fake-direct:bob']"),
      "Jamaican English voice available");
    ok(!$(".option[data-value='urn:moz-tts:fake-direct:celine']"),
      "Canadian French voice unavailable");
    ok(!$(".option[data-value='urn:moz-tts:fake-direct:julie']"),
      "Mexican Spanish voice unavailable");

    $(NarrateTestUtils.TOGGLE).click();
    ok(NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled");

    let prefChanged = NarrateTestUtils.waitForPrefChange(
      "narrate.voice", "getCharPref");
    NarrateTestUtils.selectVoice(content, "urn:moz-tts:fake-indirect:zanetta");
    let voicePref = JSON.parse(await prefChanged);
    is(voicePref.it, "urn:moz-tts:fake-indirect:zanetta", "pref set correctly");
  });
});
