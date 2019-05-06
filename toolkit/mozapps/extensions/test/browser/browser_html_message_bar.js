/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint max-len: ["error", 80] */

let htmlAboutAddonsWindow;

function clickElement(el) {
  el.dispatchEvent(new CustomEvent("click"));
}

function createMessageBar(messageBarStack, {attrs, children, onclose} = {}) {
  const win = messageBarStack.ownerGlobal;
  const messageBar = win.document.createElement("message-bar");
  if (attrs) {
    for (const [k, v] of Object.entries(attrs)) {
      messageBar.setAttribute(k, v);
    }
  }
  if (children) {
    if (Array.isArray(children)) {
      messageBar.append(...children);
    } else {
      messageBar.append(children);
    }
  }
  messageBar.addEventListener("message-bar:close", onclose, {once: true});
  messageBarStack.append(messageBar);
  return messageBar;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", true],
    ],
  });

  htmlAboutAddonsWindow = await loadInitialView("extension");
  registerCleanupFunction(async () => {
    await closeView(htmlAboutAddonsWindow);
  });
});

add_task(async function test_message_bar_stack() {
  const win = htmlAboutAddonsWindow;

  let messageBarStack = win.document.querySelector("message-bar-stack");

  ok(messageBarStack, "Got a message-bar-stack in HTML about:addons page");

  is(messageBarStack.maxMessageBarCount, 3,
     "Got the expected max-message-bar-count property");

  is(messageBarStack.childElementCount, 0,
     "message-bar-stack is initially empty");
});

add_task(async function test_create_message_bar_create_and_onclose() {
  const win = htmlAboutAddonsWindow;
  const messageBarStack = win.document.querySelector("message-bar-stack");

  let messageEl = win.document.createElement("span");
  messageEl.textContent = "A message bar text";
  let buttonEl = win.document.createElement("button");
  buttonEl.textContent = "An action button";

  let messageBar;
  let onceMessageBarClosed = new Promise(resolve => {
    messageBar = createMessageBar(messageBarStack, {
      children: [messageEl, buttonEl],
      onclose: resolve,
    });
  });

  is(messageBarStack.childElementCount, 1,
     "message-bar-stack has a child element");
  is(messageBarStack.firstElementChild, messageBar,
     "newly created message-bar added as message-bar-stack child element");

  const slot = messageBar.shadowRoot.querySelector("slot");
  is(slot.assignedNodes()[0], messageEl,
     "Got the expected span element assigned to the message-bar slot");
  is(slot.assignedNodes()[1], buttonEl,
     "Got the expected button element assigned to the message-bar slot");

  info("Click the close icon on the newly created message-bar");
  clickElement(messageBar.shadowRoot.querySelector("button.close"));

  info("Expect the onclose function to be called");
  await onceMessageBarClosed;

  is(messageBarStack.childElementCount, 0,
     "message-bar-stack has no child elements");
});

add_task(async function test_max_message_bar_count() {
  const win = htmlAboutAddonsWindow;
  const messageBarStack = win.document.querySelector("message-bar-stack");

  info("Create a new message-bar");
  let messageElement = document.createElement("span");
  messageElement = "message bar label";

  let onceMessageBarClosed = new Promise(resolve => {
    createMessageBar(messageBarStack, {
      children: messageElement,
      onclose: resolve,
    });
  });

  is(messageBarStack.childElementCount, 1,
     "message-bar-stack has the expected number of children");

  info("Create 3 more message bars");
  const allBarsPromises = [];
  for (let i = 2; i <= 4; i++) {
    allBarsPromises.push(new Promise(resolve => {
      createMessageBar(messageBarStack, {
        attrs: {dismissable: ""},
        children: [messageElement, i],
        onclose: resolve,
      });
    }));
  }

  info("Expect first message-bar to closed automatically");
  await onceMessageBarClosed;

  is(messageBarStack.childElementCount, 3,
     "message-bar-stack has the expected number of children");

  info("Click on close icon for the second message-bar");
  clickElement(messageBarStack.firstElementChild._closeIcon);

  info("Expect the second message-bar to be closed");
  await allBarsPromises[0];

  is(messageBarStack.childElementCount, 2,
     "message-bar-stack has the expected number of children");

  info("Clear the entire message-bar-stack content");
  messageBarStack.textContent = "";

  info("Expect all the created message-bar to be closed automatically");
  await Promise.all(allBarsPromises);

  is(messageBarStack.childElementCount, 0,
     "message-bar-stack has no child elements");
});
