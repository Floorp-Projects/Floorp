/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/input/doc_events.html";

add_task(async function testShiftEvents(client) {
  await setupForInput(PAGE_URL);
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
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const input = content.document.querySelector("input");
    isnot(input, content.document.activeElement, "input should lose focus");
  });
});

add_task(async function testAltEvents(client) {
  await setupForInput(PAGE_URL);
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
});

add_task(async function testControlEvents(client) {
  await setupForInput(PAGE_URL);
  const { Input } = client;

  await withModifier(Input, "Control", "ctrl", "`");
  let events = await getEvents();
  // no keypress or input event
  checkEvent(events[1], "keydown", "`", "ctrl", true);
  checkEvent(events[events.length - 1], "keyup", "Control", "ctrl", false);
});

add_task(async function testMetaEvents(client) {
  if (!isMac) {
    return;
  }
  await setupForInput(PAGE_URL);
  const { Input } = client;

  await withModifier(Input, "Meta", "meta", "a");
  let events = await getEvents();
  // no keypress or input event
  checkEvent(events[1], "keydown", "a", "meta", true);
  checkEvent(events[events.length - 1], "keyup", "Meta", "meta", false);
});

add_task(async function testShiftClick(client) {
  await loadURL(PAGE_URL);
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
});

async function getEvents() {
  const events = await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return content.eval("allEvents");
  });
  info(`Events: ${JSON.stringify(events)}`);
  return events;
}

async function resetEvents() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.eval("resetEvents()");
    const events = content.eval("allEvents");
    is(events.length, 0, "List of events should be empty");
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
