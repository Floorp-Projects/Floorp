const Cc = Components.classes;
const Ci = Components.interfaces;

const kTaskbarID = "@mozilla.org/windows-taskbar;1";
const DOWNLOAD_MANAGER_URL = "chrome://mozapps/content/downloads/downloads.xul";
const DLMGR_UI_DONE = "download-manager-ui-done";

let DownloadTaskbarProgress, TaskbarService, observerService, wwatch;
let gGen = null;

function test() {
  gGen = doTest();
  gGen.next();
}

function continueTest() {
  gGen.next();
}

function doTest() {
  let isWin7OrHigher = false;
  try {
    let version = Cc["@mozilla.org/system-info;1"]
                    .getService(Ci.nsIPropertyBag2)
                    .getProperty("version");
    isWin7OrHigher = (parseFloat(version) >= 6.1);
  } catch (ex) { }

  is(!!Win7Features, isWin7OrHigher, "Win7Features available when it should be");
  if (!isWin7OrHigher) {
    return;
  }
  waitForExplicitFinish();

  let tempScope = {};
  Cu.import("resource://gre/modules/DownloadTaskbarProgress.jsm", tempScope);

  DownloadTaskbarProgress = tempScope.DownloadTaskbarProgress;
  TaskbarService =  Cc[kTaskbarID].getService(Ci.nsIWinTaskbar);

  observerService = Cc["@mozilla.org/observer-service;1"].
                      getService(Ci.nsIObserverService);

  wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);


  isnot(DownloadTaskbarProgress, null, "Download taskbar progress service exists");
  is(TaskbarService.available, true, "Taskbar Service is available");

  //Manually call onBrowserWindowLoad because this is delayed in 10sec
  DownloadTaskbarProgress.onBrowserWindowLoad(window);


  is(DownloadTaskbarProgress.activeWindowIsDownloadWindow, false,
     "DownloadTaskbarProgress window is not the Download window");

  checkActiveTaskbar(false, window);

  openDownloadManager(continueTest);
  yield;

  let DMWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                 getService(Ci.nsIWindowMediator).
                 getMostRecentWindow("Download:Manager");

  ok(DMWindow, "Download Manager window was opened");
  checkActiveTaskbar(true, DMWindow);

  DMWindow.close();
  setTimeout(continueTest, 100);
  yield;

  checkActiveTaskbar(false, window);

  let browserWindow = openBrowserWindow(continueTest);
  yield;

  ok(browserWindow, "Browser window was opened");
  DownloadTaskbarProgress.onBrowserWindowLoad(browserWindow);

  // The owner window should not have changed, since our
  // original window still exists
  checkActiveTaskbar(false, window);

  browserWindow.close();
  finish();
}

function checkActiveTaskbar(isDownloadManager, ownerWindow) {

  isnot(DownloadTaskbarProgress.activeTaskbarProgress, null, "DownloadTaskbarProgress has an active taskbar");

  is(DownloadTaskbarProgress.activeWindowIsDownloadWindow, isDownloadManager,
     "The active taskbar progress " + (isDownloadManager ? "is" : "is not") + " the Download Manager");

  if (ownerWindow) {
    let ownerWindowDocShell = ownerWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                getInterface(Ci.nsIWebNavigation).
                                QueryInterface(Ci.nsIDocShellTreeItem).treeOwner.
                                QueryInterface(Ci.nsIInterfaceRequestor).
                                getInterface(Ci.nsIXULWindow).docShell;

    let windowTaskbarProgress = TaskbarService.getTaskbarProgress(ownerWindowDocShell);

    is(DownloadTaskbarProgress.activeTaskbarProgress, windowTaskbarProgress,
       "DownloadTaskbarProgress has the expected taskbar as active");
  }

}

function openBrowserWindow(callback) {
  let blank = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
  blank.data = "about:blank";

  browserWindow = wwatch.openWindow(null, "chrome://browser/content/",
                    "_blank", "chrome,dialog=no", blank);
 
  let helperFunc = function() {
    callback();
    browserWindow.removeEventListener("load", helperFunc, false);
  }

  browserWindow.addEventListener("load", helperFunc, false);
  return browserWindow;
}

function openDownloadManager(callback) {

  let testObs = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic != DLMGR_UI_DONE) {
        return;
      }

      callback();
      observerService.removeObserver(testObs, DLMGR_UI_DONE);
    }
  };

  observerService.addObserver(testObs, DLMGR_UI_DONE, false);

  Cc["@mozilla.org/download-manager-ui;1"].
    getService(Ci.nsIDownloadManagerUI).show();

}
