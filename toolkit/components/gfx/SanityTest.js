/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu, interfaces: Ci, classes: Cc, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/Preferences.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PAGE_WIDTH=72;
const PAGE_HEIGHT=136;
const DRIVER_PREF="sanity-test.driver-version";
const DEVICE_PREF="sanity-test.device-id";
const VERSION_PREF="sanity-test.version";
const DISABLE_VIDEO_PREF="media.hardware-video-decoding.failed";
const RUNNING_PREF="sanity-test.running";
const OS_SNAPSHOT_TIMEOUT_SEC=3;

// GRAPHICS_SANITY_TEST histogram enumeration values
const TEST_PASSED=0;
const TEST_FAILED_RENDER=1;
const TEST_FAILED_VIDEO=2;
const TEST_CRASHED=3;

// GRAPHICS_SANITY_TEST_OS_SNAPSHOT histogram enumeration values
const SNAPSHOT_VIDEO_OK=0;
const SNAPSHOT_VIDEO_FAIL=1;
const SNAPSHOT_ERROR=2;
const SNAPSHOT_TIMEOUT=3;
const SNAPSHOT_LAYERS_OK=4;
const SNAPSHOT_LAYERS_FAIL=5;

function testPixel(ctx, x, y, r, g, b, a, fuzz) {
  var data = ctx.getImageData(x, y, 1, 1);

  if (Math.abs(data.data[0] - r) <= fuzz &&
      Math.abs(data.data[1] - g) <= fuzz &&
      Math.abs(data.data[2] - b) <= fuzz &&
      Math.abs(data.data[3] - a) <= fuzz) {
    return true;
  }
  return false;
}

function reportResult(val) {
  try {
    let histogram = Services.telemetry.getHistogramById("GRAPHICS_SANITY_TEST");
    histogram.add(val);
  } catch (e) {}

  Preferences.set(RUNNING_PREF, false);
  Services.prefs.savePrefFile(null);
}

function reportSnapshotResult(val) {
  let histogram = Services.telemetry.getHistogramById("GRAPHICS_SANITY_TEST_OS_SNAPSHOT");
  histogram.add(val);
}

function takeWindowSnapshot(win, ctx) {
  // TODO: drawWindow reads back from the gpu's backbuffer, which won't catch issues with presenting
  // the front buffer via the window manager. Ideally we'd use an OS level API for reading back
  // from the desktop itself to get a more accurate test.
  var flags = ctx.DRAWWINDOW_DRAW_CARET | ctx.DRAWWINDOW_DRAW_VIEW | ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
  ctx.drawWindow(win.ownerGlobal, 0, 0, PAGE_WIDTH, PAGE_HEIGHT, "rgb(255,255,255)", flags);
}

function takeWidgetSnapshot(win, canvas, ctx) {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.drawWidgetAsOnScreen(win.ownerGlobal);
}

function testWidgetSnapshot(win, canvas, ctx) {
  try {
    takeWidgetSnapshot(win, canvas, ctx);
    if (verifyVideoRendering(ctx)) {
      reportSnapshotResult(SNAPSHOT_VIDEO_OK);
    } else {
      reportSnapshotResult(SNAPSHOT_VIDEO_FAIL);
    }

    if (verifyLayersRendering(ctx)) {
      reportSnapshotResult(SNAPSHOT_LAYERS_OK);
    } else {
      reportSnapshotResult(SNAPSHOT_LAYERS_FAIL);
    }
  } catch (e) {
    reportSnapshotResult(SNAPSHOT_ERROR);
  }
}

function setTimeout(aMs, aCallback) {
  var timer = Components.classes["@mozilla.org/timer;1"]
              .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(aCallback, aMs, Ci.nsITimer.TYPE_ONE_SHOT);
}

// Verify that all the 4 coloured squares of the video
// render as expected (with a tolerance of 64 to allow for
// yuv->rgb differences between platforms).
//
// The video is 64x64, and is split into quadrants of
// different colours. The top left of the video is 8,72
// and we test a pixel 10,10 into each quadrant to avoid
// blending differences at the edges.
//
// We allow massive amounts of fuzz for the colours since
// it can depend hugely on the yuv -> rgb conversion, and
// we don't want to fail unnecessarily.
function verifyVideoRendering(ctx) {
  return testPixel(ctx, 18, 82, 255, 255, 255, 255, 64) &&
    testPixel(ctx, 50, 82, 0, 255, 0, 255, 64) &&
    testPixel(ctx, 18, 114, 0, 0, 255, 255, 64) &&
    testPixel(ctx, 50, 114, 255, 0, 0, 255, 64);
}

// Verify that the middle of the layers test is the color we expect.
// It's a red 64x64 square, test a pixel deep into the 64x64 square
// to prevent fuzzing.
function verifyLayersRendering(ctx) {
  return testPixel(ctx, 18, 18, 255, 0, 0, 255, 64);
}

function testCompositor(win, ctx) {
  takeWindowSnapshot(win, ctx);

  if (!verifyVideoRendering(ctx)) {
    reportResult(TEST_FAILED_VIDEO);
    Preferences.set(DISABLE_VIDEO_PREF, true);
    return false;
  }

  if (!verifyLayersRendering(ctx)) {
    reportResult(TEST_FAILED_RENDER);
    return false;
  }

  reportResult(TEST_PASSED);
  return true;
}

let listener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  win: null,
  utils: null,
  canvas: null,

  // State flow:
  //   onload -> WM_PAINT -> MozAfterPaint -> ready
  //   WM_PAINT -> onload -> MozAfterPaint -> ready
  //
  // We always wait for the low-level paint message because this is what tells
  // us whether or not the OS has actually started drawing.
  windowLoaded: false,   // onload event has fired.
  windowReady: false,    // widget has received an OS-level paint request.

  scheduleTest: function(win) {
    this.win = win;
    this.win.onload = this.onWindowLoaded.bind(this);
    this.utils = this.win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);

    let observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(this, "widget-first-paint", false);
  },

  onWindowLoaded: function() {
    this.canvas = this.win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    this.canvas.setAttribute("width", PAGE_WIDTH);
    this.canvas.setAttribute("height", PAGE_HEIGHT);
    this.ctx = this.canvas.getContext("2d");

    // Perform the compositor backbuffer test, which currently we use for
    // actually deciding whether to enable hardware media decoding.
    if (!testCompositor(this.win, this.ctx)) {
      this.endTest();
      return;
    }

    // Wait to perform the OS snapshot test. Since this waits for a series of
    // events to occur, we set a timeout in case nothing happens.
    setTimeout(OS_SNAPSHOT_TIMEOUT_SEC * 1000, (() => {
      if (this.win) {
        reportSnapshotResult(SNAPSHOT_TIMEOUT);
        this.endTest();
      }
    }));

    this.windowLoaded = true;
    if (this.windowReady) {
      this.waitForPaintsFlushed();
    }
  },

  // Watch for the first OS-level paint message to the window.
  observe: function(aSubject, aTopic, aData) {
    if (aSubject != this.win || aTopic != "widget-first-paint") {
      return;
    }

    this.windowReady = true;
    if (this.windowLoaded) {
      this.waitForPaintsFlushed();
    }
  },

  // Wait for all layout-induced paints to flush.
  waitForPaintsFlushed: function() {
    // If the test ended prematurely due to a timeout, just ignore the event.
    if (!this.win) {
      return;
    }

    if (this.utils.isMozAfterPaintPending) {
      let paintListener = (() => {
        if (this.utils && this.utils.isMozAfterPaintPending) {
          return;
        }

        this.win.removeEventListener("MozAfterPaint", paintListener);

        if (this.utils) {
          // Painting is finished, we will fail the above
          // isMozAfterPaintPending test now.
          this.waitForPaintsFlushed();
        }
      });
      this.win.addEventListener("MozAfterPaint", paintListener);
      return;
    }

    testWidgetSnapshot(this.win, this.canvas, this.ctx);
    this.endTest();
  },

  endTest: function() {
    if (!this.win) {
      return;
    }

    this.win.ownerGlobal.close();
    this.win = null;
    this.utils = null;
    this.canvas = null;

    let observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Components.interfaces.nsIObserverService);
    observerService.removeObserver(this, "widget-first-paint");
  }
};

function SanityTest() {}
SanityTest.prototype = {
  classID: Components.ID("{f3a8ca4d-4c83-456b-aee2-6a2cbf11e9bd}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  shouldRunTest: function() {
    // Only test gfx features if firefox has updated, or if the user has a new
    // gpu or drivers.
    var appInfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
    var xulVersion = appInfo.version;
    var gfxinfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

    if (Preferences.get(RUNNING_PREF, false)) {
      Preferences.set(DISABLE_VIDEO_PREF, true);
      reportResult(TEST_CRASHED);
      return false;
    }

    // TODO: Handle dual GPU setups
    if (Preferences.get(DRIVER_PREF, "") == gfxinfo.adapterDriverVersion &&
        Preferences.get(DEVICE_PREF, "") == gfxinfo.adapterDeviceID &&
        Preferences.get(VERSION_PREF, "") == xulVersion) {
      return false;
    }

    // Enable hardware decoding so we can test again
    // and record the driver version to detect if the driver changes.
    Preferences.set(DISABLE_VIDEO_PREF, false);
    Preferences.set(DRIVER_PREF, gfxinfo.adapterDriverVersion);
    Preferences.set(DEVICE_PREF, gfxinfo.adapterDeviceID);
    Preferences.set(VERSION_PREF, xulVersion);

    // Update the prefs so that this test doesn't run again until the next update.
    Preferences.set(RUNNING_PREF, true);
    Services.prefs.savePrefFile(null);
    return true;
  },

  observe: function(subject, topic, data) {
    if (topic != "profile-after-change") return;
    if (!this.shouldRunTest()) return;

    // Open a tiny window to render our test page, and notify us when it's loaded
    var sanityTest = Services.ww.openWindow(null,
        "chrome://gfxsanity/content/sanitytest.html",
        "Test Page",
        "width=" + PAGE_WIDTH + ",height=" + PAGE_HEIGHT + ",chrome,titlebar=0,scrollbars=0",
        null);
    listener.scheduleTest(sanityTest);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SanityTest]);
