SimpleTest.waitForExplicitFinish();
const WIDTH = 300;
const HEIGHT = 300;
let gWindow;
let gTitlebar;

function promiseMessage(msg) {
  info(`wait for message "${msg}"`);
  return new Promise(resolve => {
    function listener(evt) {
      info(`got message "${evt.data}"`);
      if (evt.data == msg) {
        window.removeEventListener("message", listener);
        resolve();
      }
    }
    window.addEventListener("message", listener);
  });
}

function openWindow(features = "") {
  return window.browsingContext.topChromeWindow.openDialog(
    gWindow,
    "_blank",
    "chrome,dialog=no,all," + features,
    window
  );
}

function checkWindow(msg, win, sizemode, width, height) {
  is(win.windowState, sizemode, "sizemode should match " + msg);
  if (sizemode == win.STATE_NORMAL) {
    is(win.innerWidth, width, "width should match " + msg);
    is(win.innerHeight, height, "height should match " + msg);
  }
}

function todoCheckWindow(msg, win, sizemode) {
  todo_is(win.windowState, sizemode, "sizemode should match " + msg);
}

// Persistence of "sizemode" is delayed to 500ms after it's changed.
// See SIZE_PERSISTENCE_TIMEOUT in nsWebShellWindow.cpp.
// We wait for 1000ms to ensure that it is actually persisted.
// We can also wait for condition that XULStore does have the value
// set, but that way we cannot test the cases where we don't expect
// persistence to happen.
function waitForSizeModePersisted() {
  return new Promise(resolve => {
    setTimeout(resolve, 1000);
  });
}

async function changeSizeMode(func) {
  let promiseSizeModeChange = promiseMessage("sizemodechange");
  func();
  await promiseSizeModeChange;
  await waitForSizeModePersisted();
}

async function runTest(aWindow) {
  gWindow = aWindow;
  gTitlebar = aWindow != "window_maximized_persist_with_no_titlebar.xhtml";
  let win = openWindow();
  await SimpleTest.promiseFocus(win);

  // Check the default state.
  const chrome_url = win.location.href;
  checkWindow("when open initially", win, win.STATE_NORMAL, WIDTH, HEIGHT);
  const widthDiff = win.outerWidth - win.innerWidth;
  const heightDiff = win.outerHeight - win.innerHeight;
  // Maximize the window.
  await changeSizeMode(() => win.maximize());
  checkWindow("after maximize window", win, win.STATE_MAXIMIZED);
  win.close();

  // Open a new window to check persisted sizemode.
  win = openWindow();
  await SimpleTest.promiseFocus(win);
  checkWindow("when reopen to maximized", win, win.STATE_MAXIMIZED);
  // Restore the window.
  if (win.windowState == win.STATE_MAXIMIZED) {
    await changeSizeMode(() => win.restore());
  }
  checkWindow("after restore window", win, win.STATE_NORMAL, WIDTH, HEIGHT);
  win.close();

  // Open a new window again to check persisted sizemode.
  win = openWindow();
  await SimpleTest.promiseFocus(win);
  checkWindow("when reopen to normal", win, win.STATE_NORMAL, WIDTH, HEIGHT);
  // And maximize the window again for next test.
  await changeSizeMode(() => win.maximize());
  win.close();

  // Open a new window again with centerscreen which shouldn't revert
  // the persisted sizemode.
  win = openWindow("centerscreen");
  await SimpleTest.promiseFocus(win);
  checkWindow("when open with centerscreen", win, win.STATE_MAXIMIZED);
  win.close();

  // Linux doesn't seem to persist sizemode across opening window
  // with specified size, so mark it expected fail for now.
  const isLinux = navigator.platform.includes("Linux");
  let checkWindowMayFail = isLinux ? todoCheckWindow : checkWindow;

  // Open a new window with size specified.
  win = openWindow("width=400,height=400");
  await SimpleTest.promiseFocus(win);
  checkWindow("when reopen with size", win, win.STATE_NORMAL, 400, 400);
  await waitForSizeModePersisted();
  win.close();

  // Open a new window without size specified.
  // The window opened before should not change persisted sizemode.
  win = openWindow();
  await SimpleTest.promiseFocus(win);
  checkWindowMayFail("when reopen without size", win, win.STATE_MAXIMIZED);
  win.close();

  // Open a new window with sizing synchronously.
  win = openWindow();
  win.resizeTo(500 + widthDiff, 500 + heightDiff);
  await SimpleTest.promiseFocus(win);
  checkWindow("when sized synchronously", win, win.STATE_NORMAL, 500, 500);
  await waitForSizeModePersisted();
  win.close();

  // Open a new window without any sizing.
  // The window opened before should not change persisted sizemode.
  win = openWindow();
  await SimpleTest.promiseFocus(win);
  checkWindowMayFail("when reopen without sizing", win, win.STATE_MAXIMIZED);
  win.close();

  // Clean up the XUL store for the given window.
  Services.xulStore.removeDocument(chrome_url);

  SimpleTest.finish();
}
