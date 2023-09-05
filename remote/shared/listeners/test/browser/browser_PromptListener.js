/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PromptListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/PromptListener.sys.mjs"
);

add_task(async function test_without_curBrowser() {
  const listener = new PromptListener();
  const opened = listener.once("opened");
  const closed = listener.once("closed");

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  const openedEvent = await opened;

  is(openedEvent.prompt.window, dialogWin, "Received expected prompt");

  dialogWin.document.querySelector("dialog").acceptDialog();

  const closedEvent = await closed;

  is(closedEvent.detail.accepted, true, "Received correct event details");

  listener.destroy();
});

add_task(async function test_with_curBrowser() {
  const listener = new PromptListener(() => ({
    contentBrowser: gBrowser.selectedBrowser,
    window,
  }));
  const opened = listener.once("opened");
  const closed = listener.once("closed");

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  const openedEvent = await opened;

  is(openedEvent.prompt.window, dialogWin, "Received expected prompt");

  dialogWin.document.querySelector("dialog").acceptDialog();

  const closedEvent = await closed;

  is(closedEvent.detail.accepted, true, "Received correct event details");

  listener.destroy();
});

add_task(async function test_close_event_details() {
  const listener = new PromptListener();
  let closed = listener.once("closed");

  listener.startListening();

  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.prompt('Enter your name:'))`);
  let dialogWin = await dialogPromise;

  dialogWin.document.getElementById("loginTextbox").value = "Test";
  dialogWin.document.querySelector("dialog").acceptDialog();

  let closedEvent = await closed;

  is(
    closedEvent.detail.accepted,
    true,
    "Received correct `accepted` value in event details"
  );
  is(
    closedEvent.detail.userText,
    "Test",
    "Received correct `userText` value in event details"
  );

  closed = listener.once("closed");

  dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.prompt('Enter your name:'))`);
  dialogWin = await dialogPromise;

  dialogWin.document.getElementById("loginTextbox").value = "Test";
  dialogWin.document.querySelector("dialog").cancelDialog();

  closedEvent = await closed;

  is(
    closedEvent.detail.accepted,
    false,
    "Received correct `accepted` value in event details"
  );
  is(
    closedEvent.detail.userText,
    undefined,
    "Received correct `userText` value in event details"
  );

  listener.destroy();
});

add_task(async function test_dialogClosed() {
  const listener = new PromptListener();

  listener.startListening();

  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.alert('test'))`);
  let dialogWin = await dialogPromise;
  let closed = listener.dialogClosed();

  dialogWin.document.querySelector("dialog").acceptDialog();

  await closed;

  is(true, true, "Close promise got resolved");

  dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.alert('test'))`);
  dialogWin = await dialogPromise;
  closed = listener.dialogClosed();

  dialogWin.document.querySelector("dialog").cancelDialog();

  await closed;

  is(true, true, "Close promise got resolved");

  listener.destroy();
});

add_task(async function test_events_in_another_browser() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const selectedBrowser = win.gBrowser.selectedBrowser;
  const listener = new PromptListener(() => ({
    contentBrowser: selectedBrowser,
    window: selectedBrowser.ownerGlobal,
  }));
  const events = [];
  const onEvent = (name, data) => events.push(data);
  listener.on("opened", onEvent);
  listener.on("closed", onEvent);

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  ok(events.length === 0, "No event was received");

  dialogWin.document.querySelector("dialog").acceptDialog();

  // Wait a bit to make sure that the event didn't come.
  await new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, 500);
  });

  ok(events.length === 0, "No event was received");

  listener.destroy();
  await BrowserTestUtils.closeWindow(win);
});
