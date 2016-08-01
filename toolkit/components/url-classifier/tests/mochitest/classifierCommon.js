/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, results: Cr } = Components;

var dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
                .getService(Ci.nsIUrlClassifierDBService);

function setTimeout(callback, delay) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback({ notify: callback },
                           delay,
                           Ci.nsITimer.TYPE_ONE_SHOT);
}

function doUpdate(update) {
  let listener = {
    QueryInterface: function(iid)
    {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIUrlClassifierUpdateObserver))
        return this;

      throw Cr.NS_ERROR_NO_INTERFACE;
    },
    updateUrlRequested: function(url) { },
    streamFinished: function(status) { },
    updateError: function(errorCode) {
      sendAsyncMessage("updateError", errorCode);
    },
    updateSuccess: function(requestedTimeout) {
      sendAsyncMessage("updateSuccess");
    }
  };

  let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
                  .getService(Ci.nsIUrlClassifierDBService);

  try {
    dbService.beginUpdate(listener, "test-malware-simple,test-unwanted-simple", "");
    dbService.beginStream("", "");
    dbService.updateStream(update);
    dbService.finishStream();
    dbService.finishUpdate();
  } catch(e) {
    // beginUpdate may fail if there's an existing update in progress
    // retry until success or testcase timeout.
    setTimeout(() => { doUpdate(update); }, 1000);
  }
}

function doReload() {
  dbService.reloadDatabase();

  sendAsyncMessage("reloadSuccess");
}

addMessageListener("doUpdate", ({ testUpdate }) => {
  doUpdate(testUpdate);
});

addMessageListener("doReload", () => {
  doReload();
});
