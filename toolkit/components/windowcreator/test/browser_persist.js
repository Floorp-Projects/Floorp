"use strict"; // -*- js-indent-level: 2; indent-tabs-mode: nil -*-

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
  this
);

const contentBase =
  "https://example.com/browser/toolkit/components/windowcreator/test/";
const chromeBase =
  "chrome://mochitests/content/browser/toolkit/components/windowcreator/test/";

// Checks that the source and target documents are the same.
const REFTESTS = [
  "file_persist_srcset.html",
  "file_persist_picture_source.html",
  "file_persist_svg.html",
  // ...
];

async function persist(name, uri) {
  return BrowserTestUtils.withNewTab(uri, async function (browser) {
    // Snapshot the doc as loaded, this is our reference.
    info("snapshotting reference");
    let reference = await snapshotWindow(browser);

    info("starting persistence");
    let doc = await new Promise(function (resolve) {
      browser.frameLoader.startPersistence(null, {
        onDocumentReady(d) {
          resolve(d);
        },
        onError(e) {
          ok(false, "startPersistence failed: " + e);
        },
      });
    });

    let wbp = Cc[
      "@mozilla.org/embedding/browser/nsWebBrowserPersist;1"
    ].createInstance(Ci.nsIWebBrowserPersist);
    let tmp = Services.dirsvc.get("TmpD", Ci.nsIFile);
    let tmpFile = tmp.clone();
    tmpFile.append(name + "_saved.html");
    let tmpDir = tmp.clone();
    tmpDir.append(name + "_files");

    registerCleanupFunction(function cleanUp() {
      if (tmpFile.exists()) {
        tmpFile.remove(/* recursive = */ false);
      }
      if (tmpDir.exists()) {
        tmpDir.remove(/* recursive = */ true);
      }
    });

    info("persisting document");
    // Wait for the persisted document.
    await new Promise(function (resolve) {
      wbp.progressListener = {
        onProgressChange() {},
        onLocationChange() {},
        onStatusChange() {},
        onSecurityChange() {},
        onContentBlockingEvent() {},
        onStateChange(_wbp, _req, state, _status) {
          if (state & Ci.nsIWebProgressListener.STATE_STOP) {
            resolve();
          }
        },
      };

      wbp.saveDocument(doc, tmpFile, tmpDir, null, 0, 0);
    });

    info("load done, loading persisted document");
    let fileUri = Services.io.newFileURI(tmpFile).spec;
    let test = await BrowserTestUtils.withNewTab(
      fileUri,
      async function (persistedBrowser) {
        info("snapshotting persisted document");
        return snapshotWindow(persistedBrowser);
      }
    );
    return { test, reference };
  });
}

add_task(async function () {
  for (let filename of REFTESTS) {
    let uri = contentBase + filename;
    let { test, reference } = await persist(filename, uri);
    let expectEqual = true;
    let [same, testUri, refUri] = compareSnapshots(
      test,
      reference,
      expectEqual
    );
    ok(same, "test and references should match");
    if (!same) {
      info(testUri);
      info(refUri);
    }
  }
});
