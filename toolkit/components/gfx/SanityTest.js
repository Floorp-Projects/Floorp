/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu, interfaces: Ci, classes: Cc, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/Preferences.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const FRAME_SCRIPT_URL = "chrome://gfxsanity/content/gfxFrameScript.js";

const PAGE_WIDTH=92;
const PAGE_HEIGHT=166;
const DRIVER_PREF="sanity-test.driver-version";
const DEVICE_PREF="sanity-test.device-id";
const VERSION_PREF="sanity-test.version";
const DISABLE_VIDEO_PREF="media.hardware-video-decoding.failed";
const RUNNING_PREF="sanity-test.running";
const TIMEOUT_SEC=20;

// GRAPHICS_SANITY_TEST histogram enumeration values
const TEST_PASSED=0;
const TEST_FAILED_RENDER=1;
const TEST_FAILED_VIDEO=2;
const TEST_CRASHED=3;
const TEST_TIMEOUT=4;

// GRAPHICS_SANITY_TEST_REASON enumeration values.
const REASON_FIRST_RUN=0;
const REASON_FIREFOX_CHANGED=1;
const REASON_DEVICE_CHANGED=2;
const REASON_DRIVER_CHANGED=3;

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

function reportTestReason(val) {
  let histogram = Services.telemetry.getHistogramById("GRAPHICS_SANITY_TEST_REASON");
  histogram.add(val);
}

function annotateCrashReport(value) {
  try {
    // "1" if we're annotating the crash report, "" to remove the annotation.
    var crashReporter = Cc['@mozilla.org/toolkit/crash-reporter;1'].
                          getService(Ci.nsICrashReporter);
    crashReporter.annotateCrashReport("GraphicsSanityTest", value ? "1" : "");
  } catch (e) {
  }
}

function setTimeout(aMs, aCallback) {
  var timer = Cc['@mozilla.org/timer;1'].
                createInstance(Ci.nsITimer);
  timer.initWithCallback(aCallback, aMs, Ci.nsITimer.TYPE_ONE_SHOT);
}

function takeWindowSnapshot(win, ctx) {
  // TODO: drawWindow reads back from the gpu's backbuffer, which won't catch issues with presenting
  // the front buffer via the window manager. Ideally we'd use an OS level API for reading back
  // from the desktop itself to get a more accurate test.
  var flags = ctx.DRAWWINDOW_DRAW_CARET | ctx.DRAWWINDOW_DRAW_VIEW | ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
  ctx.drawWindow(win.ownerGlobal, 0, 0, PAGE_WIDTH, PAGE_HEIGHT, "rgb(255,255,255)", flags);
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
  var testPassed = true;

  if (!verifyVideoRendering(ctx)) {
    reportResult(TEST_FAILED_VIDEO);
    Preferences.set(DISABLE_VIDEO_PREF, true);
    testPassed = false;
  }

  if (!verifyLayersRendering(ctx)) {
    reportResult(TEST_FAILED_RENDER);
    testPassed = false;
  }

  if (testPassed) {
    reportResult(TEST_PASSED);
  }

  return testPassed;
}

var listener = {
  win: null,
  utils: null,
  canvas: null,
  ctx: null,
  mm: null,

  messages: [
    "gfxSanity:ContentLoaded",
  ],

  scheduleTest: function(win) {
    this.win = win;
    this.win.onload = this.onWindowLoaded.bind(this);
    this.utils = this.win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    setTimeout(TIMEOUT_SEC * 1000, () => {
      if (this.win) {
        reportResult(TEST_TIMEOUT);
        this.endTest();
      }
    });
  },

  runSanityTest: function() {
    this.canvas = this.win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    this.canvas.setAttribute("width", PAGE_WIDTH);
    this.canvas.setAttribute("height", PAGE_HEIGHT);
    this.ctx = this.canvas.getContext("2d");

    // Perform the compositor backbuffer test, which currently we use for
    // actually deciding whether to enable hardware media decoding.
    testCompositor(this.win, this.ctx);

    this.endTest();
  },

  receiveMessage(message) {
    let data = message.data;
    switch (message.name) {
      case "gfxSanity:ContentLoaded":
        this.runSanityTest();
        break;
    }
  },

  onWindowLoaded: function() {
    let browser = this.win.document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");

    let remoteBrowser = Services.appinfo.browserTabsRemoteAutostart;
    browser.setAttribute("remote", remoteBrowser);

    browser.style.width = PAGE_WIDTH + "px";
    browser.style.height = PAGE_HEIGHT + "px";

    this.win.document.documentElement.appendChild(browser);
    // Have to set the mm after we append the child
    this.mm = browser.messageManager;

    this.messages.forEach((msgName) => {
      this.mm.addMessageListener(msgName, this);
    });

    this.mm.loadFrameScript(FRAME_SCRIPT_URL, false);
  },

  endTest: function() {
    if (!this.win) {
      return;
    }

    this.win.ownerGlobal.close();
    this.win = null;
    this.utils = null;
    this.canvas = null;
    this.ctx = null;

    if (this.mm) {
      // We don't have a MessageManager if onWindowLoaded never fired.
      this.messages.forEach((msgName) => {
        this.mm.removeMessageListener(msgName, this);
      });

      this.mm = null;
    }

    // Remove the annotation after we've cleaned everything up, to catch any
    // incidental crashes from having performed the sanity test.
    annotateCrashReport(false);
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
    var buildId = Services.appinfo.platformBuildID;
    var gfxinfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

    if (Preferences.get(RUNNING_PREF, false)) {
      Preferences.set(DISABLE_VIDEO_PREF, true);
      reportResult(TEST_CRASHED);
      return false;
    }

    function checkPref(pref, value, reason) {
      var prefValue = Preferences.get(pref, undefined);
      if (prefValue == value) {
        return true;
      }
      if (prefValue === undefined) {
        reportTestReason(REASON_FIRST_RUN);
      } else {
        reportTestReason(reason);
      }
      return false;
    }

    // TODO: Handle dual GPU setups
    if (checkPref(DRIVER_PREF, gfxinfo.adapterDriverVersion, REASON_DRIVER_CHANGED) &&
        checkPref(DEVICE_PREF, gfxinfo.adapterDeviceID, REASON_DEVICE_CHANGED) &&
        checkPref(VERSION_PREF, buildId, REASON_FIREFOX_CHANGED))
    {
      return false;
    }

    // Enable hardware decoding so we can test again
    // and record the driver version to detect if the driver changes.
    Preferences.set(DISABLE_VIDEO_PREF, false);
    Preferences.set(DRIVER_PREF, gfxinfo.adapterDriverVersion);
    Preferences.set(DEVICE_PREF, gfxinfo.adapterDeviceID);
    Preferences.set(VERSION_PREF, buildId);

    // Update the prefs so that this test doesn't run again until the next update.
    Preferences.set(RUNNING_PREF, true);
    Services.prefs.savePrefFile(null);
    return true;
  },

  observe: function(subject, topic, data) {
    if (topic != "profile-after-change") return;

    // profile-after-change fires only at startup, so we won't need
    // to use the listener again.
    let tester = listener;
    listener = null;

    if (!this.shouldRunTest()) return;

    annotateCrashReport(true);

    // Open a tiny window to render our test page, and notify us when it's loaded
    var sanityTest = Services.ww.openWindow(null,
        "chrome://gfxsanity/content/sanityparent.html",
        "Test Page",
        "width=" + PAGE_WIDTH + ",height=" + PAGE_HEIGHT + ",chrome,titlebar=0,scrollbars=0",
        null);

    // There's no clean way to have an invisible window and ensure it's always painted.
    // Instead, move the window far offscreen so it doesn't show up during launch.
    sanityTest.moveTo(100000000, 1000000000);
    tester.scheduleTest(sanityTest);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SanityTest]);
