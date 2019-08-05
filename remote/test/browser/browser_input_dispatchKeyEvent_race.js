/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Here we test that the `dispatchKeyEvent` API resolves after all the synchronous event
// handlers from the content page have been flushed.
//
// Say the content page has an event handler such as:
//
//   el.addEventListener("keyup", () => {
//     doSomeVeryLongProcessing(); // <- takes a long time but is synchronous!
//     window.myVariable = "newValue";
//   });
//
// And imagine this is tested via:
//
//   await Input.dispatchKeyEvent(...);
//   const myVariable = await Runtime.evaluate({ expression: "window.myVariable" });
//   equals(myVariable, "newValue");
//
// In order for this to work, we need to be sure that `await Input.dispatchKeyEvent`
// resolves only after the content page flushed the event handlers (and
// `window.myVariable = "newValue"` was executed).
//
// This can be racy because Input.dispatchKeyEvent and window.myVariable = "newValue" run
// in different processes.

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/doc_input_dispatchKeyEvent_race.html";

add_task(async function() {
  const { client, tab } = await setupForURL(PAGE_URL);
  is(gBrowser.selectedTab, tab, "Selected tab is the target tab");

  const { Input, Runtime } = client;

  // Need an enabled Runtime domain to run evaluate.
  info("Enable the Runtime domain");
  await Runtime.enable();
  const { context } = await Runtime.executionContextCreated();

  info("Focus the input on the page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const input = content.document.querySelector("input");
    input.focus();
    is(input, content.document.activeElement, "Input should be focused");
  });

  // See doc_input_dispatchKeyEvent_race.html
  // The page listens to `input` events to update a property on window.
  // We will check that the value is updated as soon dispatchKeyEvent has resolved.
  await checkWindowTestValue("initial-value", context.id, Runtime);

  info("Write 'hhhhhh' ('h' times 6)");
  for (let i = 0; i < 6; i++) {
    await dispatchKeyEvent(Input, "h", 72, "keyDown");
    await dispatchKeyEvent(Input, "h", 72, "keyUp");
  }
  await checkWindowTestValue("hhhhhh", context.id, Runtime);

  info("Write 'aaaaaa' with 6 consecutive keydown and one keyup");
  await Promise.all([
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
    dispatchKeyEvent(Input, "a", 65, "keyDown"),
  ]);
  await dispatchKeyEvent(Input, "a", 65, "keyUp");
  await checkWindowTestValue("hhhhhhaaaaaa", context.id, Runtime);

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(tab);

  await RemoteAgent.close();
});

function dispatchKeyEvent(Input, key, keyCode, type, modifiers = 0) {
  info(`Send ${type} for key ${key}`);
  return Input.dispatchKeyEvent({
    type,
    modifiers,
    windowsVirtualKeyCode: keyCode,
    key,
  });
}

async function checkWindowTestValue(expected, contextId, Runtime) {
  info("Retrieve the value of `window.testValue` in the test page");
  const { result } = await Runtime.evaluate({
    contextId,
    expression: "window.testValue",
  });

  is(result.value, expected, "Content window test value is correct");
}
