/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function waitForReflow(aWindow) {
  return new Promise(resolve => {
    aWindow.requestAnimationFrame(() => {
      aWindow.requestAnimationFrame(resolve);
    });
  });
}

add_task(async function fullscreen_size() {
  let win = await BrowserTestUtils.openNewBrowserWindow({});
  win.gBrowser.selectedBrowser.focus();

  info("Enter browser fullscreen mode");
  let promise = Promise.all([
    BrowserTestUtils.waitForEvent(win, "fullscreen"),
    BrowserTestUtils.waitForEvent(win, "resize"),
  ]);
  win.fullScreen = true;
  await promise;

  info("Await reflow of the chrome window");
  await waitForReflow(win);

  is(win.innerHeight, win.outerHeight, "Check height");
  is(win.innerWidth, win.outerWidth, "Check width");

  await BrowserTestUtils.closeWindow(win);
});

// https://bugzilla.mozilla.org/show_bug.cgi?id=1830721
add_task(async function fullscreen_size_moz_appearance() {
  const win = await BrowserTestUtils.openNewBrowserWindow({});
  win.gBrowser.selectedBrowser.focus();

  info("Add -moz-appearance style to chrome document");
  const style = win.document.createElement("style");
  style.innerHTML = `
    #main-window {
      -moz-appearance: -moz-win-borderless-glass;
    }
  `;
  win.document.head.appendChild(style);

  info("Await reflow of the chrome window");
  await waitForReflow(win);

  info("Enter browser fullscreen mode");
  let promise = Promise.all([
    BrowserTestUtils.waitForEvent(win, "fullscreen"),
    BrowserTestUtils.waitForEvent(win, "resize"),
  ]);
  win.fullScreen = true;
  await promise;

  info("Await reflow of the chrome window");
  await waitForReflow(win);

  is(win.innerHeight, win.outerHeight, "Check height");
  is(win.innerWidth, win.outerWidth, `Check width`);

  await BrowserTestUtils.closeWindow(win);
});

// https://bugzilla.mozilla.org/show_bug.cgi?id=1800416
add_task(async function fullscreen_size_minimize_restore() {
  async function waitForSizeMode(aWindow, aSizeMode) {
    await BrowserTestUtils.waitForEvent(
      aWindow,
      "sizemodechange",
      false,
      () => {
        return aWindow.windowState === aSizeMode;
      }
    );
    const expectedHidden =
      aSizeMode == aWindow.STATE_MINIMIZED || aWindow.isFullyOccluded;
    if (aWindow.document.hidden != expectedHidden) {
      await BrowserTestUtils.waitForEvent(aWindow, "visibilitychange");
    }
    is(
      aWindow.document.hidden,
      expectedHidden,
      "Should be inactive if minimized or occluded"
    );
  }

  const win = await BrowserTestUtils.openNewBrowserWindow({});
  win.gBrowser.selectedBrowser.focus();

  info("Enter browser fullscreen mode");
  let promise = Promise.all([
    BrowserTestUtils.waitForEvent(win, "fullscreen"),
    BrowserTestUtils.waitForEvent(win, "resize"),
  ]);
  win.fullScreen = true;
  await promise;

  info("Await reflow of the chrome window");
  await waitForReflow(win);

  is(win.innerHeight, win.outerHeight, "Check height");
  is(win.innerWidth, win.outerWidth, `Check width`);

  info("Minimize window");
  promise = waitForSizeMode(win, win.STATE_MINIMIZED);
  win.minimize();
  await promise;

  info("Restore window");
  promise = waitForSizeMode(win, win.STATE_FULLSCREEN);
  win.focus();
  await promise;

  info("Await reflow of the chrome window");
  await waitForReflow(win);

  is(win.innerHeight, win.outerHeight, "Check height");
  is(win.innerWidth, win.outerWidth, `Check width`);

  await BrowserTestUtils.closeWindow(win);
});
