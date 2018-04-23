/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const server = new HttpServer();
server.registerDirectory("/", do_get_cwd());
server.start(-1);
const ROOT = `http://localhost:${server.identity.primaryPort}`;
const BASE = `${ROOT}/`;
const HEADLESS_URL = `${BASE}/headless.html`;
const HEADLESS_BUTTON_URL = `${BASE}/headless_button.html`;
registerCleanupFunction(() => { server.stop(() => {})});

// Refrences to the progress listeners to keep them from being gc'ed
// before they are called.
const progressListeners = new Map();

function loadContentWindow(webNavigation, uri) {
  return new Promise((resolve, reject) => {
    webNavigation.loadURI(uri, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    let docShell = webNavigation.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDocShell);
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebProgress);
    let progressListener = {
      onLocationChange: function (progress, request, location, flags) {
        // Ignore inner-frame events
        if (progress != webProgress) {
          return;
        }
        // Ignore events that don't change the document
        if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
          return;
        }
        let docShell = webNavigation.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);
        let contentWindow = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow);
        webProgress.removeProgressListener(progressListener);
        progressListeners.delete(progressListener);
        contentWindow.addEventListener("load", (event) => {
          resolve(contentWindow);
        }, { once: true });
      },
      QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
                                             "nsISupportsWeakReference"])
    };
    progressListeners.set(progressListener, progressListener);
    webProgress.addProgressListener(progressListener,
                                    Ci.nsIWebProgress.NOTIFY_LOCATION);
  });
}

add_task(async function test_snapshot() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  let webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
  let contentWindow = await loadContentWindow(webNavigation, HEADLESS_URL);
  const contentWidth = 400;
  const contentHeight = 300;
  // Verify dimensions.
  contentWindow.resizeTo(contentWidth, contentHeight);
  equal(contentWindow.innerWidth, contentWidth);
  equal(contentWindow.innerHeight, contentHeight);

  // Snapshot the test page.
  let canvas = contentWindow.document.createElementNS('http://www.w3.org/1999/xhtml', 'html:canvas');
  let context = canvas.getContext('2d');
  let width = contentWindow.innerWidth;
  let height = contentWindow.innerHeight;
  canvas.width = width;
  canvas.height = height;
  context.drawWindow(
    contentWindow,
    0,
    0,
    width,
    height,
    'rgb(255, 255, 255)'
  );
  let imageData = context.getImageData(0, 0, width, height).data;
  ok(imageData[0] === 0 && imageData[1] === 255 && imageData[2] === 0 && imageData[3] === 255, "Page is green.");

  // Search for a blue pixel (a quick and dirty check to see if the blue text is
  // on the page)
  let found = false;
  for (let i = 0; i < imageData.length; i += 4) {
    if (imageData[i + 2] === 255) {
      found = true;
      break;
    }
  }
  ok(found, "Found blue text on page.");

  webNavigation.close();
});

add_task(async function test_snapshot_widget_layers() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  let webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
  let contentWindow = await loadContentWindow(webNavigation, HEADLESS_URL);
  const contentWidth = 1;
  const contentHeight = 2;
  // Verify dimensions.
  contentWindow.resizeTo(contentWidth, contentHeight);
  equal(contentWindow.innerWidth, contentWidth);
  equal(contentWindow.innerHeight, contentHeight);

  // Snapshot the test page.
  let canvas = contentWindow.document.createElementNS('http://www.w3.org/1999/xhtml', 'html:canvas');
  let context = canvas.getContext('2d');
  let width = contentWindow.innerWidth;
  let height = contentWindow.innerHeight;
  canvas.width = width;
  canvas.height = height;
  context.drawWindow(
    contentWindow,
    0,
    0,
    width,
    height,
    'rgb(255, 255, 255)',
    context.DRAWWINDOW_DRAW_CARET | context.DRAWWINDOW_DRAW_VIEW | context.DRAWWINDOW_USE_WIDGET_LAYERS
  );
  ok(true, "Snapshot with widget layers didn't crash.");

  webNavigation.close();
});

// Ensure keydown events are triggered on the windowless browser.
add_task(async function test_keydown() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  let webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
  let contentWindow = await loadContentWindow(webNavigation, HEADLESS_URL);

  let keydown = new Promise((resolve) => {
    contentWindow.addEventListener("keydown", () => {
      resolve();
    }, { once: true });
  })

  let tip = Cc["@mozilla.org/text-input-processor;1"]
            .createInstance(Ci.nsITextInputProcessor);
  let begun = tip.beginInputTransactionForTests(contentWindow);
  ok(begun, "nsITextInputProcessor.beginInputTransactionForTests() should succeed");
  tip.keydown(new contentWindow.KeyboardEvent("", {key: "a", code: "KeyA", keyCode: contentWindow.KeyboardEvent.DOM_VK_A}));

  await keydown;
  ok(true, "Send keydown didn't crash");

  webNavigation.close();
});

// Test dragging the mouse on a button to ensure the creation of the drag
// service doesn't crash in headless.
add_task(async function test_mouse_drag() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  let webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
  let contentWindow = await loadContentWindow(webNavigation, HEADLESS_BUTTON_URL);
  contentWindow.resizeTo(400, 400);

  let target = contentWindow.document.getElementById('btn');
  let rect = target.getBoundingClientRect();
  let left = rect.left;
  let top = rect.top;

  let utils = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
  utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
  utils.sendMouseEvent("mousemove", left, top, 0, 1, 0, false, 0, 0);
  // Wait for a turn of the event loop since the synthetic mouse event
  // that creates the drag service is processed during the refresh driver.
  await new Promise((r) => {executeSoon(r)});
  utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);

  ok(true, "Send mouse event didn't crash");

  webNavigation.close();
});
