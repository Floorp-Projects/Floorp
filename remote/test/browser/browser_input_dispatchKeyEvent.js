/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Basic test for dispatch key event API with input type text

const TEST_URI = "data:text/html;charset=utf-8,<input type=text>";

// Map of key codes used in this test.
const KEYCODES = {
  "a": 65,
  "Backspace": 8,
  "h": 72,
  "H": 72,
  "AltLeft": 18,
  "ArrowLeft": 37,
  "ArrowRight": 39,
};

// Modifier for move forward shortcut is CTRL+RightArrow on Linux/Windows, ALT+RightArrow
// on Mac.
const isMac = Services.appinfo.OS === "Darwin";
const ALT_MODIFIER = 1;
const CTRL_MODIFIER = 2;
const ARROW_MODIFIER = isMac ? ALT_MODIFIER : CTRL_MODIFIER;

add_task(async function() {
  const { client, tab } = await setupTestForUri(TEST_URI);
  is(gBrowser.selectedTab, tab, "Selected tab is the target tab");

  const { Input } = client;

  info("Focus the input on the page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const input = content.document.querySelector("input");
    input.focus();
    is(input, content.document.activeElement, "Input should be focused");
  });
  await checkInputContent("", 0);

  info("Write 'h'");
  await dispatchKeyEvent(Input, "h", "keyDown");
  await dispatchKeyEvent(Input, "h", "keyUp");
  await checkInputContent("h", 1);

  info("Write 'H'");
  await dispatchKeyEvent(Input, "H", "keyDown");
  await dispatchKeyEvent(Input, "H", "keyUp");
  await checkInputContent("hH", 2);

  info("Send char type event for char [’]");
  await Input.dispatchKeyEvent({
    type: "char",
    modifiers: 0,
    key: "’",
  });
  await checkInputContent("hH’", 3);

  info("Send Left");
  await dispatchKeyEvent(Input, "ArrowLeft", "rawKeyDown");
  await dispatchKeyEvent(Input, "ArrowLeft", "keyUp");
  await checkInputContent("hH’", 2);


  info("Write 'a'");
  await dispatchKeyEvent(Input, "a", "keyDown");
  await dispatchKeyEvent(Input, "a", "keyUp");
  await checkInputContent("hHa’", 3);

  info("Send Left");
  await dispatchKeyEvent(Input, "ArrowLeft", "rawKeyDown");
  await dispatchKeyEvent(Input, "ArrowLeft", "keyUp");
  await checkInputContent("hHa’", 2);

  // Normally puppeteer will only non text events with rawKeyDown, but since we treat both
  // identically in Firefox, we test ArrowLeft with the regular keyDown here to show that
  // it still works.
  info("Send Left using keyDown instead of rawKeyDown");
  await dispatchKeyEvent(Input, "ArrowLeft", "keyDown");
  await dispatchKeyEvent(Input, "ArrowLeft", "keyUp");
  await checkInputContent("hHa’", 1);

  info("Write 'a'");
  await dispatchKeyEvent(Input, "a", "keyDown");
  await dispatchKeyEvent(Input, "a", "keyUp");
  await checkInputContent("haHa’", 2);

  info("Send ALT + Right");
  await dispatchKeyEvent(Input, "ArrowRight", "rawKeyDown", ARROW_MODIFIER);
  await dispatchKeyEvent(Input, "ArrowRight", "keyUp", ARROW_MODIFIER);
  await checkInputContent("haHa’", 5);

  info("Delete every character in the input");
  await sendBackspace(Input, "haHa");
  await sendBackspace(Input, "haH");
  await sendBackspace(Input, "ha");
  await sendBackspace(Input, "h");
  await sendBackspace(Input, "");

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(tab);

  await RemoteAgent.close();
});

function dispatchKeyEvent(Input, key, type, modifiers = 0) {
  info(`Send ${type} for key ${key}`);
  return Input.dispatchKeyEvent({
    type,
    modifiers,
    windowsVirtualKeyCode: KEYCODES[key],
    key,
  });
}

async function checkInputContent(expectedValue, expectedCaret) {
  const { value, caret } =
    await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
      const input = content.document.querySelector("input");
      return { value: input.value, caret: input.selectionStart };
    });

  is(value, expectedValue, `The input value is correct ("${value}"="${expectedValue}")`);
  is(caret, expectedCaret,
    `The input caret has the correct index ("${caret}"="${expectedCaret}")`);
}

async function sendBackspace(Input, expected) {
  info("Send Backspace");
  await dispatchKeyEvent(Input, "Backspace", "rawKeyDown");
  await dispatchKeyEvent(Input, "Backspace", "keyUp");
  await checkInputContent(expected, expected.length);
}
