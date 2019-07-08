/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(teardown);

add_task(async function testNarrate() {
  setup("urn:moz-tts:fake:teresa");

  await spawnInNewReaderTab(TEST_ARTICLE, async function() {
    let $ = content.document.querySelector.bind(content.document);

    await NarrateTestUtils.waitForNarrateToggle(content);

    let popup = $(NarrateTestUtils.POPUP);
    ok(!NarrateTestUtils.isVisible(popup), "popup is initially hidden");

    let toggle = $(NarrateTestUtils.TOGGLE);
    toggle.click();

    ok(NarrateTestUtils.isVisible(popup), "popup toggled");

    NarrateTestUtils.isStoppedState(content, ok);

    let promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
    $(NarrateTestUtils.START).click();
    let voice = (await promiseEvent).detail.voice;
    is(voice, "urn:moz-tts:fake:teresa", "double-check voice");

    // Skip forward to first paragraph.
    let details;
    do {
      promiseEvent = ContentTaskUtils.waitForEvent(content, "paragraphstart");
      $(NarrateTestUtils.FORWARD).click();
      details = (await promiseEvent).detail;
    } while (details.tag != "p");

    let boundaryPat = /(\S+)/g;
    let position = { left: 0, top: 0 };
    let text = details.paragraph;
    for (let res = boundaryPat.exec(text); res; res = boundaryPat.exec(text)) {
      promiseEvent = ContentTaskUtils.waitForEvent(content, "wordhighlight");
      NarrateTestUtils.sendBoundaryEvent(
        content,
        "word",
        res.index,
        res[0].length
      );
      let { start, end } = (await promiseEvent).detail;
      let nodes = NarrateTestUtils.getWordHighlights(content);
      for (let node of nodes) {
        // Since this is English we can assume each word is to the right or
        // below the previous one.
        ok(
          node.left > position.left || node.top > position.top,
          "highlight position is moving"
        );
        position = { left: node.left, top: node.top };
      }
      let wordFromOffset = text.substring(start, end);
      // XXX: Each node should contain the part of the word it highlights.
      // Right now, each node contains the entire word.
      let wordFromHighlight = nodes[0].word;
      is(wordFromOffset, wordFromHighlight, "Correct word is highlighted");
    }

    $(NarrateTestUtils.STOP).click();
    await ContentTaskUtils.waitForCondition(
      () => !$(NarrateTestUtils.STOP),
      "transitioned to stopped state"
    );
    NarrateTestUtils.isWordHighlightGone(content, ok);
  });
});
