/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-script */

var dbService = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
  Ci.nsIUrlClassifierDBService
);
var listmanager = Cc["@mozilla.org/url-classifier/listmanager;1"].getService(
  Ci.nsIUrlListManager
);

var timer;
function setTimeout(callback, delay) {
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    { notify: callback },
    delay,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}

function doUpdate(update) {
  let listener = {
    QueryInterface: ChromeUtils.generateQI(["nsIUrlClassifierUpdateObserver"]),
    updateUrlRequested(url) {},
    streamFinished(status) {},
    updateError(errorCode) {
      sendAsyncMessage("updateError", errorCode);
    },
    updateSuccess(requestedTimeout) {
      sendAsyncMessage("updateSuccess");
    },
  };

  try {
    dbService.beginUpdate(
      listener,
      "test-malware-simple,test-unwanted-simple",
      ""
    );
    dbService.beginStream("", "");
    dbService.updateStream(update);
    dbService.finishStream();
    dbService.finishUpdate();
  } catch (e) {
    // beginUpdate may fail if there's an existing update in progress
    // retry until success or testcase timeout.
    setTimeout(() => {
      doUpdate(update);
    }, 1000);
  }
}

function doReload() {
  try {
    dbService.reloadDatabase();
    sendAsyncMessage("reloadSuccess");
  } catch (e) {
    setTimeout(() => {
      doReload();
    }, 1000);
  }
}

// SafeBrowsing.jsm is initialized after mozEntries are added. Add observer
// to receive "finished" event. For the case when this function is called
// after the event had already been notified, we lookup entries to see if
// they are already added to database.
function waitForInit() {
  if (listmanager.isRegistered()) {
    sendAsyncMessage("safeBrowsingInited");
  } else {
    setTimeout(() => {
      waitForInit();
    }, 1000);
  }
}

addMessageListener("doUpdate", ({ testUpdate }) => {
  doUpdate(testUpdate);
});

addMessageListener("doReload", () => {
  doReload();
});

addMessageListener("waitForInit", () => {
  waitForInit();
});
