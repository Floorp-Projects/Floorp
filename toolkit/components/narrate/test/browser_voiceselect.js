/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(teardown);

add_task(async function testVoiceselectDropdownAutoclose() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    $(NarrateTestUtils.TOGGLE).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled"
    );

    ok(
      !NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options are initially hidden"
    );

    $(NarrateTestUtils.VOICE_SELECT).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options are toggled"
    );

    $(NarrateTestUtils.TOGGLE).click();
    // A focus will follow a real click.
    $(NarrateTestUtils.TOGGLE).focus();
    ok(
      !NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "narrate popup is dismissed"
    );

    $(NarrateTestUtils.TOGGLE).click();
    // A focus will follow a real click.
    $(NarrateTestUtils.TOGGLE).focus();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "narrate popup is showing again"
    );
    ok(
      !NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options are hidden after popup comes back"
    );
  });
});

add_task(async function testVoiceselectLabelChange() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    $(NarrateTestUtils.TOGGLE).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled"
    );

    ok(
      NarrateTestUtils.selectVoice(content, "urn:moz-tts:fake:lenny"),
      "voice selected"
    );

    let selectedOption = $(NarrateTestUtils.VOICE_SELECTED);
    let selectLabel = $(NarrateTestUtils.VOICE_SELECT_LABEL);

    is(
      selectedOption.textContent,
      selectLabel.textContent,
      "new label matches selected voice"
    );
  });
});

add_task(async function testVoiceselectKeyboard() {
  setup();

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    $(NarrateTestUtils.TOGGLE).click();
    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.POPUP)),
      "popup is toggled"
    );

    let eventUtils = NarrateTestUtils.getEventUtils(content);

    let firstValue = $(NarrateTestUtils.VOICE_SELECTED).dataset.value;

    ok(
      !NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options initially are hidden"
    );

    $(NarrateTestUtils.VOICE_SELECT).focus();

    eventUtils.synthesizeKey("KEY_ArrowDown", {}, content);

    await ContentTaskUtils.waitForCondition(
      () => $(NarrateTestUtils.VOICE_SELECTED).dataset.value != firstValue,
      "value changed after pressing ArrowDown key"
    );

    eventUtils.synthesizeKey("KEY_Enter", {}, content);

    ok(
      NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options showing after pressing Enter"
    );

    eventUtils.synthesizeKey("KEY_ArrowUp", {}, content);

    eventUtils.synthesizeKey("KEY_Enter", {}, content);

    ok(
      !NarrateTestUtils.isVisible($(NarrateTestUtils.VOICE_OPTIONS)),
      "voice options hidden after pressing Enter"
    );

    await ContentTaskUtils.waitForCondition(
      () => $(NarrateTestUtils.VOICE_SELECTED).dataset.value == firstValue,
      "value changed back to original after pressing Enter"
    );
  });
});
