/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";
const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const server = new HttpServer();
server.registerDirectory("/", do_get_cwd());
server.start(-1);
const ROOT = `http://localhost:${server.identity.primaryPort}`;
const BASE = `${ROOT}/`;
const HEADLESS_URL = `${BASE}/headless.html`;
const HEADLESS_BUTTON_URL = `${BASE}/headless_button.html`;
do_register_cleanup(() => { server.stop(() => {})});

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
        contentWindow.addEventListener("load", (event) => {
          resolve(contentWindow);
        }, { once: true });
      },
      QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
                                            "nsISupportsWeakReference"])
    };
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

// Ensure keydown events are triggered on the windowless browser.
add_task(async function test_keydown() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  let webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
  let contentWindow = await loadContentWindow(webNavigation, HEADLESS_URL);

  let utils = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
  let keydown = new Promise((resolve) => {
    contentWindow.addEventListener("keydown", () => {
      resolve();
    }, { once: true });
  })
  utils.sendKeyEvent("keydown", 65, 65, 0);

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
  await new Promise((r) => {do_execute_soon(r)});
  utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);

  ok(true, "Send mouse event didn't crash");

  webNavigation.close();
});
