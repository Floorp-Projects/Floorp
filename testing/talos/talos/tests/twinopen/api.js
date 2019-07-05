"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TalosParentProfiler",
  "resource://talos-powers/TalosParentProfiler.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

var OPENER_DELAY = 1000; // ms delay between tests

async function openDelay(win) {
  return new Promise(resolve => {
    win.setTimeout(resolve, OPENER_DELAY);
  });
}

function waitForBrowserPaint() {
  return new Promise(resolve => {
    let observer = {
      observe(doc) {
        if (
          !doc.location ||
          doc.location.href != AppConstants.BROWSER_CHROME_URL
        ) {
          return;
        }
        Services.obs.removeObserver(observer, "document-element-inserted");
        doc.ownerGlobal.addEventListener(
          "MozAfterPaint",
          evt => {
            resolve(
              doc.ownerGlobal.performance.timing.fetchStart + evt.paintTimeStamp
            );
          },
          { once: true }
        );
      },
    };
    Services.obs.addObserver(observer, "document-element-inserted");
  });
}

async function startTest(context) {
  await TalosParentProfiler.resume("twinopen", true);
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
  let win = context.xulWindow;
  await openDelay(win);
  let mozAfterPaint = waitForBrowserPaint();

  // We have to compare time measurements across two windows so we must use
  // the absolute time.
  let start = win.performance.timing.fetchStart + win.performance.now();
  let newWin = win.OpenBrowserWindow();
  let end = await mozAfterPaint;
  newWin.close();
  return end - start;
}

/* globals ExtensionAPI */
this.twinopen = class extends ExtensionAPI {
  getAPI(context) {
    return {
      twinopen: {
        startTest: () => startTest(context),
      },
    };
  }
};
