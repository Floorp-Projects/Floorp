/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();
  start_httpserver();

  debugDump("testing resuming a download on startup that was in progress " +
            "when the application quit");

  // Simulate a download that was partially completed in an earlier session
  // by writing part of the final MAR into the download path.
  const marBytesToPreCopy = 200;
  let marFile = getTestDirFile(FILE_SIMPLE_MAR);
  let marBytes = readFileBytes(marFile);
  marBytes = marBytes.substring(0, marBytesToPreCopy);
  let updateFile = getUpdatesPatchDir();
  updateFile.append(FILE_UPDATE_MAR);
  writeFile(updateFile, marBytes);

  // Also need to spoof the entityID that the HttpBaseChannel would have created
  // had those bytes been transferred by a real channel.
  let marModTime = new Date(marFile.lastModifiedTime).toUTCString();
  let patchProps = {state: STATE_DOWNLOADING,
                    url: gURLData + FILE_SIMPLE_MAR,
                    entityID: "/" + SIZE_SIMPLE_MAR + "/" + marModTime};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {appVersion: "1.0"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  standardInit();

  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.activeUpdate.state, STATE_DOWNLOADING,
               "the update manager activeUpdate state attribute" +
               MSG_SHOULD_EQUAL);

  gAUS.addDownloadListener({
    onStartRequest(aRequest, aContext) { },

    onStatus(aRequest, aContext, aStatus, aStatusText) { },

    onProgress(aRequest, aContext, aProgress, aMaxProgress) {
      aRequest.QueryInterface(Ci.nsIHttpChannel);
      Assert.equal(aRequest.getResponseHeader("Content-Length"),
                   SIZE_SIMPLE_MAR - marBytesToPreCopy,
                   "the downloader should only try to transfer the bytes " +
                   "that were not already present");
    },

    onStopRequest(aRequest, aContext, aStatus) {
      Assert.equal(aStatus, Cr.NS_OK, "the download should succeed");

      gUpdateManager.cleanupActiveUpdate();
      stop_httpserver(doTestFinish);
    },

    QueryInterface(iid) {
      if (iid.equals(Ci.nsIRequestObserver) ||
          iid.equals(Ci.nsIProgressEventSink)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  });
}
