/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict"; // -*- js-indent-level: 2; indent-tabs-mode: nil -*-
const contentBase =
  "https://example.com/browser/toolkit/components/windowcreator/test/";
const chromeBase =
  "chrome://mochitests/content/browser/toolkit/components/windowcreator/test/";
const testPageURL = contentBase + "bug1204626_doc0.html";

function one_test(delay, continuation) {
  let delayStr = delay === null ? "no delay" : "delay = " + delay + "ms";
  let browser;

  BrowserTestUtils.openNewForegroundTab(gBrowser, testPageURL).then(tab => {
    browser = tab.linkedBrowser;
    let persistable = browser.frameLoader;
    persistable.startPersistence(null, {
      onDocumentReady,
      onError(status) {
        ok(false, new Components.Exception("startPersistence failed", status));
        continuation();
      },
    });
  });

  function onDocumentReady(doc) {
    const nameStem = "test_bug1204626_" + Date.now();
    let wbp = Cc[
      "@mozilla.org/embedding/browser/nsWebBrowserPersist;1"
    ].createInstance(Ci.nsIWebBrowserPersist);
    let tmp = Services.dirsvc.get("TmpD", Ci.nsIFile);
    let tmpFile = tmp.clone();
    tmpFile.append(nameStem + "_saved.html");
    let tmpDir = tmp.clone();
    tmpDir.append(nameStem + "_files");

    registerCleanupFunction(function cleanUp() {
      if (tmpFile.exists()) {
        tmpFile.remove(/* recursive: */ false);
      }
      if (tmpDir.exists()) {
        tmpDir.remove(/* recursive: */ true);
      }
    });

    wbp.progressListener = {
      onProgressChange() {},
      onLocationChange() {},
      onStatusChange() {},
      onSecurityChange() {},
      onContentBlockingEvent() {},
      onStateChange(_wbp, _req, state, _status) {
        if ((state & Ci.nsIWebProgressListener.STATE_STOP) == 0) {
          return;
        }
        ok(true, "Finished save (" + delayStr + ") but might have crashed.");
        continuation();
      },
    };

    function doSave() {
      wbp.saveDocument(doc, tmpFile, tmpDir, null, 0, 0);
    }
    if (delay === null) {
      doSave();
    } else {
      setTimeout(doSave, delay);
    }
    SpecialPowers.spawn(browser, [], () => {
      content.window.close();
    });
  }
}

function test() {
  waitForExplicitFinish();
  // 0ms breaks having the actor under PBrowser, but not 10ms.
  // 10ms provokes the double-__delete__, but not 0ms.
  // And a few others, just in case.
  const testRuns = [null, 0, 10, 0, 10, 20, 50, 100];
  let i = 0;
  (function next_test() {
    if (i < testRuns.length) {
      one_test(testRuns[i++], next_test);
    } else {
      finish();
    }
  })();
}
