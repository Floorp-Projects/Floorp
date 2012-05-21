/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests data uri downloading of the DM in relation to the new security policy
// checks put in place on windows. (bug 416683)

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function run_test()
{
  // Don't finish until the download is finished
  do_test_pending();

  function addDownload() {
    const nsIWBP = Ci.nsIWebBrowserPersist;
    var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);
    persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                           nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                           nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

    // Download to a temp local file
    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("policychecktest.png");
    if (file.exists())
      file.remove(false);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

    gDownloadCount++;

    var dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                            createURI("data:image/png;base64,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"),
                            createURI(file), null, null,
                            Math.round(Date.now() * 1000), null, persist);

    persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
    persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

    return dl;
  }

  let listener = {
    onDownloadStateChange: function(aState, aDownload)
    {
      switch (aDownload.state) {
        case dm.DOWNLOAD_FAILED:
        case dm.DOWNLOAD_CANCELED:
        case dm.DOWNLOAD_DIRTY:
        case dm.DOWNLOAD_BLOCKED_POLICY:
          // Fail!
          if (aDownload.targetFile.exists())
            aDownload.targetFile.remove(false);
          dm.removeListener(this);
          do_throw("data: uri failed to download successfully");
          do_test_finished();
          break;

        case dm.DOWNLOAD_FINISHED:
          do_check_true(aDownload.targetFile.exists());
          aDownload.targetFile.remove(false);
          dm.removeListener(this);
          do_test_finished();
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { }
  };

  dm.addListener(listener);
  dm.addListener(getDownloadListener());

  addDownload();
}
