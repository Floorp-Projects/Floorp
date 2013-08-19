/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  try {
    if (Services.prefs.getBoolPref("browser.download.useJSTransfer")) {
      return;
    }
  } catch (ex) { }

  waitForExplicitFinish();

  let privateWin = null;
  let itemCount = 0;
  let sourceURL =
    "http://example.org/tests/toolkit/components/downloads/test/browser/download.html";
  let linkURL =
    "http://mochi.test:8888/tests/toolkit/components/downloads/test/browser/download.test";
  let linkURI = Services.io.newURI(linkURL, null, null);
  let downloadDialogURL =
    "chrome://mozapps/content/downloads/unknownContentType.xul";

  let downloadListener = {
    onDownloadStateChange: function(aState, aDownload) {
      switch (aDownload.state) {
        case Services.downloads.DOWNLOAD_FINISHED:
          info("Download finished");
          Services.downloads.removeListener(downloadListener);
          executeSoon(function() { checkDownload(aDownload); });
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };

  let historyObserver = {
    onVisit: function (aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
      ok(false, "Download should not fired a visit notification: " + aURI.spec);
    },
    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onTitleChanged: function () {},
    onBeforeDeleteURI: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };

  let windowListener = {
    onOpenWindow: function(aXULWindow) {
      info("Window opened");
      Services.wm.removeListener(windowListener);

      let domWindow =
        aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor).
          getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        is(domWindow.document.location.href, downloadDialogURL,
           "Should have seen the right window open");

        executeSoon(function() {
          let button = domWindow.document.documentElement.getButton("accept");
          button.disabled = false;
          domWindow.document.documentElement.acceptDialog();
        });
      }, domWindow);
    },
    onCloseWindow: function(aXULWindow) {},
    onWindowTitleChange: function(aXULWindow, aNewTitle) {}
  };

  registerCleanupFunction(function() {
    privateWin.close();
    Services.prefs.clearUserPref("browser.download.manager.showAlertOnComplete");
  });

  function getHistoryItemCount() {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.includeHidden = true;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
    let query = PlacesUtils.history.getNewQuery();
    let root = PlacesUtils.history.executeQuery(query, options).root;
    root.containerOpen = true;
    let cc = root.childCount;
    root.containerOpen = false;
    return cc;
  }

  function whenNewWindowLoaded(aIsPrivate, aCallback) {
    let win = OpenBrowserWindow({private: aIsPrivate});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() aCallback(win));
    }, false);
  }

  function whenPageLoad(aWin, aURL, aCallback) {
    let browser = aWin.gBrowser.selectedBrowser;
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      executeSoon(function() aCallback(browser.contentDocument));
    }, true);
    browser.loadURI(aURL);
  }

  function checkDownload(aDownload) {
    PlacesUtils.history.removeObserver(historyObserver);
    ok(aDownload.isPrivate, "Download should be private");
    is(getHistoryItemCount(), itemCount,
       "History items count should not change after a download");
    PlacesUtils.asyncHistory.isURIVisited(linkURI, function(aURI, aIsVisited) {
      is(aIsVisited, false, "Download source should not be set as visited");
      // Clean up
      if (aDownload.targetFile.exists()) {
        aDownload.targetFile.remove(false);
      }
      waitForFocus(function() {
        if (privateWin.DownloadsPanel.isPanelShowing) {
          privateWin.DownloadsPanel.hidePanel();
        }
        finish();
      }, privateWin);
    });
  }

  // Disable alert service notifications
  Services.prefs.setBoolPref("browser.download.manager.showAlertOnComplete", false);

  whenNewWindowLoaded(true, function(win) {
    info("Start listeners");
    privateWin = win;
    Services.wm.addListener(windowListener);
    Services.downloads.addPrivacyAwareListener(downloadListener);
    PlacesUtils.history.addObserver(historyObserver, false);
    info("Load test page");
    whenPageLoad(win, sourceURL, function(doc) {
      info("Start download");
      itemCount = getHistoryItemCount();
      doc.getElementById("download-link").click();
    });
  });
}
