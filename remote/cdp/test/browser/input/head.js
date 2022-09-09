/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
  this
);

const { Input: I } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/parent/Input.jsm"
);
const { AppInfo } = ChromeUtils.import(
  "chrome://remote/content/shared/AppInfo.jsm"
);

const { alt, ctrl, meta, shift } = I.Modifier;

// Map of key codes used in Input tests.
const KEYCODES = {
  a: KeyboardEvent.DOM_VK_A,
  A: KeyboardEvent.DOM_VK_A,
  b: KeyboardEvent.DOM_VK_B,
  B: KeyboardEvent.DOM_VK_B,
  c: KeyboardEvent.DOM_VK_C,
  C: KeyboardEvent.DOM_VK_C,
  h: KeyboardEvent.DOM_VK_H,
  H: KeyboardEvent.DOM_VK_H,
  Alt: KeyboardEvent.DOM_VK_ALT,
  ArrowLeft: KeyboardEvent.DOM_VK_LEFT,
  ArrowRight: KeyboardEvent.DOM_VK_RIGHT,
  ArrowDown: KeyboardEvent.DOM_VK_DOWN,
  Backspace: KeyboardEvent.DOM_VK_BACK_SPACE,
  Control: KeyboardEvent.DOM_VK_CONTROL,
  Meta: KeyboardEvent.DM_VK_META,
  Shift: KeyboardEvent.DOM_VK_SHIFT,
  Tab: KeyboardEvent.DOM_VK_TAB,
};

async function setupForInput(url) {
  await loadURL(url);
  info("Focus the input on the page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const input = content.document.querySelector("input");
    input.focus();
    is(input, content.document.activeElement, "Input should be focused");
    is(input.value, "", "Check input content");
    is(input.selectionStart, 0, "Check position of input caret");
  });
}

async function withModifier(Input, modKey, mod, key) {
  await dispatchKeyEvent(Input, modKey, "rawKeyDown", I.Modifier[mod]);
  await dispatchKeyEvent(Input, key, "keyDown", I.Modifier[mod]);
  await dispatchKeyEvent(Input, key, "keyUp", I.Modifier[mod]);
  await dispatchKeyEvent(Input, modKey, "keyUp");
}

function dispatchKeyEvent(Input, key, type, modifiers = 0) {
  info(`Send ${type} for key ${key}`);
  return Input.dispatchKeyEvent({
    type,
    modifiers,
    windowsVirtualKeyCode: KEYCODES[key],
    key,
  });
}

async function getEvents() {
  const events = await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return content.wrappedJSObject.allEvents;
  });
  info(`Events: ${JSON.stringify(events)}`);
  return events;
}

function getInputContent() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const input = content.document.querySelector("input");
    return { value: input.value, caret: input.selectionStart };
  });
}

function checkEvent(event, type, key, property, expectedValue) {
  let expected = { type, key };
  expected[property] = expectedValue;
  checkProperties(expected, event, "Event");
}

async function checkInputContent(expectedValue, expectedCaret) {
  const { value, caret } = await getInputContent();
  is(value, expectedValue, "Check input content");
  is(caret, expectedCaret, "Check position of input caret");
}

function checkProperties(expectedObj, targetObj, message = "Compare objects") {
  for (const prop in expectedObj) {
    is(targetObj[prop], expectedObj[prop], message + `: check ${prop}`);
  }
}

function keyForPlatform() {
  // TODO add cases for other key-combinations as the need arises
  let primary = ctrl;
  let primaryKey = "Control";
  if (AppInfo.isMac) {
    primary = alt;
    primaryKey = "Alt";
  }
  return { primary, primaryKey };
}

async function sendTextKey(Input, key, modifiers = 0) {
  await dispatchKeyEvent(Input, key, "keyDown", modifiers);
  await dispatchKeyEvent(Input, key, "keyUp", modifiers);
}

async function sendText(Input, text) {
  for (const sym of text) {
    await sendTextKey(Input, sym);
  }
}

async function sendRawKey(Input, key, modifiers = 0) {
  await dispatchKeyEvent(Input, key, "rawKeyDown", modifiers);
  await dispatchKeyEvent(Input, key, "keyUp", modifiers);
}

async function checkBackspace(Input, expected, modifiers = 0) {
  info("Send Backspace");
  await sendRawKey(Input, "Backspace", modifiers);
  await checkInputContent(expected, expected.length);
}

async function resetEvents() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.resetEvents();
    const events = content.wrappedJSObject.allEvents;
    is(events.length, 0, "List of events should be empty");
  });
}

function resetInput(value = "") {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [value], function(arg) {
    const input = content.document.querySelector("input");
    input.value = arg;
    input.focus();
  });
}
