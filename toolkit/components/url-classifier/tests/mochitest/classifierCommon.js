/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function doUpdate(update) {
  const { classes: Cc, interfaces: Ci, results: Cr } = Components;

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

  dbService.beginUpdate(listener, "test-malware-simple,test-unwanted-simple", "");
  dbService.beginStream("", "");
  dbService.updateStream(update);
  dbService.finishStream();
  dbService.finishUpdate();
}

addMessageListener("doUpdate", ({ testUpdate }) => {
  doUpdate(testUpdate);
});
