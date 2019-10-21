/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Input: I } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/Input.jsm"
);

const { alt, ctrl, meta, shift } = I.Modifier;

// Map of key codes used in this test.
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

const isMac = Services.appinfo.OS === "Darwin";

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/doc_input_events.html";

add_task(async function testTypingPrintableCharacters() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  info("Write 'h'");
  await sendTextKey(Input, "h");
  await checkInputContent("h", 1);

  info("Write 'H'");
  await sendTextKey(Input, "H");
  await checkInputContent("hH", 2);

  info("Send char type event for char [’]");
  await Input.dispatchKeyEvent({
    type: "char",
    modifiers: 0,
    key: "’",
  });
  await checkInputContent("hH’", 3);

  await teardown(client);
});

add_task(async function testArrowKeys() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  await sendText(Input, "hH’");
  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hH’", 2);

  info("Write 'a'");
  await sendTextKey(Input, "a");
  await checkInputContent("hHa’", 3);

  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hHa’", 2);

  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hHa’", 1);

  info("Write 'a'");
  await sendTextKey(Input, "a");
  await checkInputContent("haHa’", 2);

  info("Send ALT/CONTROL + Right");
  let modCode = isMac ? alt : ctrl;
  let modKey = isMac ? "Alt" : "Control";
  await dispatchKeyEvent(Input, modKey, "rawKeyDown", modCode);
  await dispatchKeyEvent(Input, "ArrowRight", "rawKeyDown", modCode);
  await dispatchKeyEvent(Input, "ArrowRight", "keyUp");
  await dispatchKeyEvent(Input, modKey, "keyUp");
  await checkInputContent("haHa’", 5);

  await teardown(client);
});

add_task(async function testBackspace() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  await sendText(Input, "haHa’");

  info("Delete every character in the input");
  await checkBackspace(Input, "haHa");
  await checkBackspace(Input, "haH");
  await checkBackspace(Input, "ha");
  await checkBackspace(Input, "h");
  await checkBackspace(Input, "");

  await teardown(client);
});

add_task(async function testShiftSelect() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Shift + Left (select one char to the left)");
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  info("(deleteContentBackward)");
  await checkBackspace(Input, "word 2 wo");
  await dispatchKeyEvent(Input, "Shift", "keyUp");

  await resetInput("word 2 wo");
  info("Send Shift + Left (select one char to the left)");
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendTextKey(Input, "H");
  await checkInputContent("word 2 H", 8);
  await dispatchKeyEvent(Input, "Shift", "keyUp");

  await teardown(client);
});

add_task(async function testSelectWord() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Shift + Ctrl/Alt + Left (select one word to the left)");
  const { primary, primaryKey } = keyForPlatform();
  const combined = shift | primary;
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await dispatchKeyEvent(Input, primaryKey, "rawKeyDown", combined);
  await sendRawKey(Input, "ArrowLeft", combined);
  await sendRawKey(Input, "ArrowLeft", combined);
  await dispatchKeyEvent(Input, "Shift", "keyUp", primary);
  await dispatchKeyEvent(Input, primaryKey, "keyUp");
  info("(deleteContentBackward)");
  await checkBackspace(Input, "word ");

  await teardown(client);
});

add_task(async function testSelectDelete() {
  const { client } = await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Ctrl/Alt + Backspace (deleteWordBackward)");
  const { primary, primaryKey } = keyForPlatform();
  await dispatchKeyEvent(Input, primaryKey, "rawKeyDown", primary);
  await checkBackspace(Input, "word 2 ", primary);
  await dispatchKeyEvent(Input, primaryKey, "keyUp");

  await resetInput("word 2 ");
  await sendText(Input, "word4");
  await sendRawKey(Input, "ArrowLeft");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("word 2 word4", 10);

  if (isMac) {
    info("Send Meta + Backspace (deleteSoftLineBackward)");
    await dispatchKeyEvent(Input, "Meta", "rawKeyDown", meta);
    await sendRawKey(Input, "Backspace", meta);
    await dispatchKeyEvent(Input, "Meta", "keyUp");
    await checkInputContent("d4", 0);
  }

  await teardown(client);
});

add_task(async function testShiftEvents() {
  const { client } = await setupForInput(PAGE_URL);
  const { Input } = client;
  await resetEvents();

  await withModifier(Input, "Shift", "shift", "A");
  await checkInputContent("A", 1);
  let events = await getEvents();
  checkEvent(events[0], "keydown", "Shift", "shift", true);
  checkEvent(events[1], "keydown", "A", "shift", true);
  checkEvent(events[2], "keypress", "A", "shift", true);
  checkProperties({ data: "A", inputType: "insertText" }, events[3]);
  checkEvent(events[4], "keyup", "A", "shift", true);
  checkEvent(events[5], "keyup", "Shift", "shift", false);
  await resetEvents();

  await withModifier(Input, "Shift", "shift", "Enter");
  events = await getEvents();
  checkEvent(events[2], "keypress", "Enter", "shift", true);
  await resetEvents();

  await withModifier(Input, "Shift", "shift", "Tab");
  events = await getEvents();
  checkEvent(events[1], "keydown", "Tab", "shift", true);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const input = content.document.querySelector("input");
    isnot(input, content.document.activeElement, "input should lose focus");
  });

  await teardown(client);
});

add_task(async function testAltEvents() {
  const { client } = await setupForInput(PAGE_URL);
  const { Input } = client;

  await withModifier(Input, "Alt", "alt", "a");
  if (isMac) {
    await checkInputContent("a", 1);
  } else {
    await checkInputContent("", 0);
  }
  let events = await getEvents();
  checkEvent(events[1], "keydown", "a", "alt", true);
  checkEvent(events[events.length - 1], "keyup", "Alt", "alt", false);
  await teardown(client);
});

add_task(async function testControlEvents() {
  const { client } = await setupForInput(PAGE_URL);
  const { Input } = client;

  await withModifier(Input, "Control", "ctrl", "`");
  let events = await getEvents();
  // no keypress or input event
  checkEvent(events[1], "keydown", "`", "ctrl", true);
  checkEvent(events[events.length - 1], "keyup", "Control", "ctrl", false);
  await teardown(client);
});

add_task(async function testMetaEvents() {
  if (!isMac) {
    return;
  }
  const { client } = await setupForInput(PAGE_URL);
  const { Input } = client;

  await withModifier(Input, "Meta", "meta", "a");
  let events = await getEvents();
  // no keypress or input event
  checkEvent(events[1], "keydown", "a", "meta", true);
  checkEvent(events[events.length - 1], "keyup", "Meta", "meta", false);

  await teardown(client);
});

add_task(async function testCtrlShiftArrows() {
  const { client } = await setupForURL(
    toDataURL('<select multiple size="3"><option>a<option>b<option>c</select>')
  );
  const { Input } = client;

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const select = content.document.querySelector("select");
    select.selectedIndex = 0;
    select.focus();
  });

  const combined = shift | ctrl;
  await dispatchKeyEvent(Input, "Control", "rawKeyDown", shift);
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", combined);
  await sendRawKey(Input, "ArrowDown", combined);
  await dispatchKeyEvent(Input, "Control", "keyUp", shift);
  await dispatchKeyEvent(Input, "Shift", "keyUp");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const select = content.document.querySelector("select");
    ok(select[0].selected, "First option should be selected");
    ok(select[1].selected, "Second option should be selected");
    ok(!select[2].selected, "Third option should not be selected");
  });
  await teardown(client);
});

add_task(async function testShiftClick() {
  const { client } = await setupForURL(PAGE_URL);
  const { Input } = client;
  await resetEvents();

  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  info("Click the 'pointers' div.");
  await Input.dispatchMouseEvent({
    type: "mousePressed",
    x: 80,
    y: 180,
    modifiers: shift,
  });
  await Input.dispatchMouseEvent({
    type: "mouseReleased",
    x: 80,
    y: 180,
    modifiers: shift,
  });
  await dispatchKeyEvent(Input, "Shift", "keyUp", shift);
  let events = await getEvents();
  checkProperties({ type: "click", shiftKey: true, button: 0 }, events[2]);

  await teardown(client);
});

function keyForPlatform() {
  // TODO add cases for other key-combinations as the need arises
  let primary = ctrl;
  let primaryKey = "Control";
  if (isMac) {
    primary = alt;
    primaryKey = "Alt";
  }
  return { primary, primaryKey };
}

async function setupForInput(url) {
  const { client, tab } = await setupForURL(url);
  info("Focus the input on the page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const input = content.document.querySelector("input");
    input.focus();
    is(input, content.document.activeElement, "Input should be focused");
  });
  await checkInputContent("", 0);
  return { client, tab };
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

function getInputContent() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const input = content.document.querySelector("input");
    return { value: input.value, caret: input.selectionStart };
  });
}

async function checkInputContent(expectedValue, expectedCaret) {
  const { value, caret } = await getInputContent();
  is(value, expectedValue, "Check input content");
  is(caret, expectedCaret, "Check position of input caret");
}

// assuming doc_input_events.html
async function getEvents() {
  const events = await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    return content.eval("allEvents");
  });
  info(`Events: ${JSON.stringify(events)}`);
  return events;
}

// assuming doc_input_events.html
async function resetEvents() {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.eval("resetEvents()");
    const events = content.eval("allEvents");
    is(events.length, 0, "List of events should be empty");
  });
}

function resetInput(value = "") {
  return ContentTask.spawn(gBrowser.selectedBrowser, value, function(arg) {
    const input = content.document.querySelector("input");
    input.value = arg;
    input.focus();
  });
}

function checkEvent(event, type, key, property, expectedValue) {
  let expected = { type, key };
  expected[property] = expectedValue;
  checkProperties(expected, event, "Event");
}

function checkProperties(expectedObj, targetObj, message = "Compare objects") {
  for (const prop in expectedObj) {
    is(targetObj[prop], expectedObj[prop], message + `: check ${prop}`);
  }
}
