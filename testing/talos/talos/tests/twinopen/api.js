"use strict";

/* globals AppConstants, Services */

ChromeUtils.defineESModuleGetters(this, {
  TalosParentProfiler: "resource://talos-powers/TalosParentProfiler.sys.mjs",
});

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
  TalosParentProfiler.subtestStart("twinopen");
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
  let win = context.appWindow;
  await openDelay(win);
  let mozAfterPaint = waitForBrowserPaint();

  // We have to compare time measurements across two windows so we must use
  // the absolute time.
  let start = win.performance.timing.fetchStart + win.performance.now();
  let newWin = win.OpenBrowserWindow();
  let end = await mozAfterPaint;
  TalosParentProfiler.subtestEnd("twinopen");
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
