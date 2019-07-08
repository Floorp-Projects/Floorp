/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(teardown);

add_task(async function testVoiceselectDropdownAutoclose() {
  setup("automatic", true);

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    ok(
      !!$(".option[data-value='urn:moz-tts:fake:bob']"),
      "Jamaican English voice available"
    );
    ok(
      !!$(".option[data-value='urn:moz-tts:fake:lenny']"),
      "Canadian English voice available"
    );
    ok(
      !!$(".option[data-value='urn:moz-tts:fake:amy']"),
      "British English voice available"
    );

    ok(
      !$(".option[data-value='urn:moz-tts:fake:celine']"),
      "Canadian French voice unavailable"
    );
    ok(
      !$(".option[data-value='urn:moz-tts:fake:julie']"),
      "Mexican Spanish voice unavailable"
    );

    $(NarrateTestUtils.TOGGLE).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled"
    );

    let prefChanged = NarrateTestUtils.waitForPrefChange(
      "narrate.voice",
      "getCharPref"
    );
    NarrateTestUtils.selectVoice(content, "urn:moz-tts:fake:lenny");
    let voicePref = JSON.parse(await prefChanged);
    is(voicePref.en, "urn:moz-tts:fake:lenny", "pref set correctly");
  });
});

add_task(async function testVoiceselectDropdownAutoclose() {
  setup("automatic", true);

  await spawnInNewReaderTab(TEST_ITALIAN_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    ok(
      !!$(".option[data-value='urn:moz-tts:fake:zanetta']"),
      "Italian voice available"
    );
    ok(
      !!$(".option[data-value='urn:moz-tts:fake:margherita']"),
      "Italian voice available"
    );

    ok(
      !$(".option[data-value='urn:moz-tts:fake:bob']"),
      "Jamaican English voice available"
    );
    ok(
      !$(".option[data-value='urn:moz-tts:fake:celine']"),
      "Canadian French voice unavailable"
    );
    ok(
      !$(".option[data-value='urn:moz-tts:fake:julie']"),
      "Mexican Spanish voice unavailable"
    );

    $(NarrateTestUtils.TOGGLE).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled"
    );

    let prefChanged = NarrateTestUtils.waitForPrefChange(
      "narrate.voice",
      "getCharPref"
    );
    NarrateTestUtils.selectVoice(content, "urn:moz-tts:fake:zanetta");
    let voicePref = JSON.parse(await prefChanged);
    is(voicePref.it, "urn:moz-tts:fake:zanetta", "pref set correctly");
  });
});
