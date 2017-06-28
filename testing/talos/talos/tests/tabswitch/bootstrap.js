
// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-

/* globals APP_SHUTDOWN */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/RemotePageManager.jsm");

let aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                           .getService(Ci.nsIAboutNewTabService);

var aboutBlankTab = null;
let context = {};
let TalosParentProfiler;

var windowListener = {
  onOpenWindow(aWindow) {
    // Ensure we don't get tiles which contact the network
    aboutNewTabService.newTabURL = "about:blank";

    // Wait for the window to finish loading
    let window = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowInternal || Ci.nsIDOMWindow);
    let cb = function() {
      window.removeEventListener("load", cb);
      loadIntoWindow(window);
    };
    window.addEventListener("load", cb);
  },

  onCloseWindow(aWindow) {
    aboutNewTabService.resetNewTabURL();
  },

  onWindowTitleChange(aWindow, aTitle) {
  }
};

function promiseOneEvent(target, eventName, capture) {
  let deferred = Promise.defer();
  target.addEventListener(eventName, function handler(event) {
    deferred.resolve();
  }, {capture, once: true});
  return deferred.promise;
}

function executeSoon(callback) {
  Services.tm.dispatchToMainThread(callback);
}

/**
 * Returns a Promise that resolves when browser-delayed-startup-finished
 * fires for a given window
 *
 * @param win
 *        The window that we're waiting for the notification for.
 * @returns Promise
 */
function waitForDelayedStartup(win) {
  return new Promise((resolve) => {
    const topic = "browser-delayed-startup-finished";
    Services.obs.addObserver(function onStartup(subject) {
      if (win == subject) {
        Services.obs.removeObserver(onStartup, topic);
        resolve();
      }
    }, topic);
  });
}

/**
 * For some <xul:tabbrowser>, loads a collection of URLs as new tabs
 * in that browser.
 *
 * @param gBrowser (<xul:tabbrowser>)
 *        The <xul:tabbrowser> in which to load the new tabs.
 * @param urls (Array)
 *        An array of URL strings to be loaded as new tabs.
 * @returns Promise
 *        Resolves once all tabs have finished loading.
 */
function loadTabs(gBrowser, urls) {
  return new Promise((resolve) => {
    gBrowser.loadTabs(urls, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    let waitingToLoad = new Set(urls);

    let listener = {
      QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
                                             "nsISupportsWeakReference"]),
      onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
        let loadedState = Ci.nsIWebProgressListener.STATE_STOP |
          Ci.nsIWebProgressListener.STATE_IS_NETWORK;
        if ((aStateFlags & loadedState) == loadedState &&
            !aWebProgress.isLoadingDocument &&
            aWebProgress.isTopLevel &&
            Components.isSuccessCode(aStatus)) {

          dump(`Loaded: ${aBrowser.currentURI.spec}\n`);
          waitingToLoad.delete(aBrowser.currentURI.spec);

          if (!waitingToLoad.size) {
            gBrowser.removeTabsProgressListener(listener);
            dump("Loads complete - starting tab switches\n");
            resolve();
          }
        }
      },
    };

    gBrowser.addTabsProgressListener(listener);
  });
}

/**
 * Loads the utility content script for the tps for out-of-process
 * browsers into a browser. This should not be used for in-process
 * browsers.
 *
 * The utility script will send a "TPS:ContentSawPaint" message
 * through the browser's message manager when it sees that its
 * content has been presented to the user.
 *
 * @param browser (<xul:browser>)
 *        The remote browser to load the script in.
 * @returns Promise
 *        Resolves once the script has been loaded and executed in
 *        the remote browser.
 */
function loadTPSContentScript(browser) {
  if (!browser.isRemoteBrowser) {
    throw new Error("loadTPSContentScript expects a remote browser.");
  }
  return new Promise((resolve) => {
    // Here's our utility script. We'll serialize this and send it down
    // to run in the content process for this browser.
    let script = function() {
      let Cu = Components.utils;
      let Ci = Components.interfaces;
      Cu.import("resource://gre/modules/Services.jsm");

      /**
       * In order to account for the fact that a MozAfterPaint might fire
       * for a composite that's unrelated to this tab's content being
       * painted, we'll get the last used layer transaction ID for
       * this content's refresh driver, and make sure that the MozAfterPaint
       * that we react to has a greater transaction id.
       *
       * Note also that this comment needs to stay inside this comment
       * block. No // comments allowed when serializing JS to content
       * scripts this way.
       */
      let cwu = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
      let lastTransactionId = cwu.lastTransactionId;
      Services.profiler.AddMarker("Content waiting for id > " + lastTransactionId);
      addEventListener("MozAfterPaint", function onPaint(event) {
        Services.profiler.AddMarker("Content saw transaction id: " + event.transactionId);
        if (event.transactionId > lastTransactionId) {
          Services.profiler.AddMarker("Content saw correct MozAfterPaint");
          sendAsyncMessage("TPS:ContentSawPaint", {
            time: Date.now().valueOf(),
          });
          removeEventListener("MozAfterPaint", onPaint);
        }
      });

      sendAsyncMessage("TPS:ContentReady");
    };

    let mm = browser.messageManager;
    mm.loadFrameScript("data:,(" + script.toString() + ")();", true);
    mm.addMessageListener("TPS:ContentReady", function onReady() {
      mm.removeMessageListener("TPS:ContentReady", onReady);
      resolve();
    });
  });
}

/**
 * For some <xul:tab> in a browser window, have that window switch
 * to that tab. Returns a Promise that resolves ones the tab content
 * has been presented to the user.
 */
function switchToTab(tab) {
  let browser = tab.linkedBrowser;
  let gBrowser = tab.ownerGlobal.gBrowser;

  // Single-process tab switching works quite differently from
  // multi-process tab switching. In the single-process case, tab
  // switching is synchronous, whereas in the multi-process case,
  // it is not. The following two tab switching mechanisms encapsulate
  // those two differences.

  if (browser.isRemoteBrowser) {
    return Task.spawn(function*() {
      // The multi-process case requires that we load our utility script
      // inside the content, since it's the content that will hear a MozAfterPaint
      // once the content is presented to the user.
      yield loadTPSContentScript(browser);
      let start = Date.now().valueOf();
      TalosParentProfiler.resume("start (" + start + "): " + browser.currentURI.spec);

      // We need to wait for the TabSwitchDone event to make sure
      // that the async tab switcher has shut itself down.
      let switchDone = waitForTabSwitchDone(browser);
      // Set up our promise that will wait for the content to be
      // presented.
      let finishPromise = waitForContentPresented(browser);
      // Finally, do the tab switch.
      gBrowser.selectedTab = tab;

      yield switchDone;
      let finish = yield finishPromise;
      TalosParentProfiler.mark("end (" + finish + ")");
      return finish - start;
    });
  }

  return Task.spawn(function*() {
    let win = browser.ownerGlobal;
    let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);

    let start = Date.now().valueOf();
    TalosParentProfiler.resume("start (" + start + "): " + browser.currentURI.spec);

    // There is no async tab switcher for the single-process case,
    // but tabbrowser.xml will still fire this once the updateCurrentBrowser
    // method runs.
    let switchDone = waitForTabSwitchDone(browser);
    // Do our tab switch
    gBrowser.selectedTab = tab;
    // Because the above tab switch is synchronous, we know that the
    // we want a MozAfterPaint with a greater layer transaction id than
    // what is currently the "last transaction id" for the window.
    let lastTransactionId = winUtils.lastTransactionId;

    yield switchDone;

    // Now we'll wait for content to be presented. Because
    // this is the single-process case, we pass the last transaction
    // id that we got so that we don't get any intermediate MozAfterPaint's
    // that might fire before web content is shown.
    let finish = yield waitForContentPresented(browser, lastTransactionId);
    TalosParentProfiler.mark("end (" + finish + ")");
    return finish - start;
  });
}

/**
 * For some <xul:browser>, find the <xul:tabbrowser> associated with it,
 * and wait until that tabbrowser has finished a tab switch. This function
 * assumes a tab switch has started, or is about to start.
 *
 * @param browser (<xul:browser>)
 *        The browser whose tabbrowser we expect to be involved in a tab
 *        switch.
 * @returns Promise
 *        Resolves once the TabSwitchDone event is fired.
 */
function waitForTabSwitchDone(browser) {
  return new Promise((resolve) => {
    let gBrowser = browser.ownerGlobal.gBrowser;
    gBrowser.addEventListener("TabSwitchDone", function onTabSwitchDone() {
      resolve();
    }, {once: true});
  });
}

/**
 * For some <xul:browser>, returns a Promise that resolves once its
 * content has been presented to the user.
 *
 * @param browser (<xul:browser>)
 *        The browser we expect to be presented.
 * @param lastTransactionId (int, optional)
 *        In the single-process case, we need to know the last layer
 *        transaction id that was used before the switch started. That
 *        way, when the MozAfterPaint fires, we can be sure that its
 *        transaction id is greater than the one that was last used,
 *        so we know that the content has definitely been presented.
 *
 *        This argument is ignored in the multi-process browser case.
 *
 * @returns Promise
 *        Resolves once the content has been presented. Resolves to
 *        the system time that the presentation occurred at, in
 *        milliseconds since midnight 01 January, 1970 UTC.
 */
function waitForContentPresented(browser, lastTransactionId) {
  // We treat multi-process browsers differently here - we expect the
  // utility script we loaded to inform us once content has been presented.
  if (browser.isRemoteBrowser) {
    return new Promise((resolve) => {
      let mm = browser.messageManager;
      mm.addMessageListener("TPS:ContentSawPaint", function onContentPaint(msg) {
        mm.removeMessageListener("TPS:ContentSawPaint", onContentPaint);
        resolve(msg.data.time);
      });
    });
  }

  // Wait for the next MozAfterPaint for this browser's window that has
  // a greater transaction id than lastTransactionId.
  return new Promise((resolve) => {
    let win = browser.ownerGlobal;
    win.addEventListener("MozAfterPaint", function onPaint(event) {
      if (event instanceof Ci.nsIDOMNotifyPaintEvent) {
        TalosParentProfiler.mark("Content saw transaction id: " + event.transactionId);
        if (event.transactionId > lastTransactionId) {
          win.removeEventListener("MozAfterPaint", onPaint);
          TalosParentProfiler.mark("Content saw MozAfterPaint");
          resolve(Date.now().valueOf());
        }
      }
    });
  });
}

/**
 * Given some browser, do a garbage collect in the parent, and then
 * a garbage collection in the content process that the browser is
 * running in.
 *
 * @param browser (<xul:browser>)
 *        The browser in which to do the garbage collection.
 * @returns Promise
 *        Resolves once garbage collection has been completed in the
 *        parent, and the content process for the browser (if applicable).
 */
function forceGC(win, browser) {
  // TODO: Find a better way of letting Talos force GC in the child. We're
  // stealing a chunk of pageloader to do this, and we should probably put
  // something into TalosPowers instead.
  browser.messageManager.loadFrameScript("chrome://pageloader/content/talos-content.js", false);

  win.QueryInterface(Ci.nsIInterfaceRequestor)
     .getInterface(Ci.nsIDOMWindowUtils)
     .garbageCollect();

  return new Promise((resolve) => {
    let mm = browser.messageManager;
    mm.addMessageListener("Talos:ForceGC:OK", function onTalosContentForceGC(msg) {
      mm.removeMessageListener("Talos:ForceGC:OK", onTalosContentForceGC);
      resolve();
    });
    mm.sendAsyncMessage("Talos:ForceGC");
  });
}

/**
 * Given some host window, open a new window, browser its initial tab to
 * about:blank, then load up our set of testing URLs. Once they've all finished
 * loading, switch through each tab, recording their tab switch times. Finally,
 * report the results.
 *
 * @param window
 *        A host window. Primarily, we just use this for the OpenBrowserWindow
 *        function defined in that window.
 * @returns Promise
 */
function test(window) {
  Services.scriptloader.loadSubScript("chrome://talos-powers-content/content/TalosParentProfiler.js", context);
  TalosParentProfiler = context.TalosParentProfiler;

  return Task.spawn(function*() {
    let testURLs = [];

    let win = window.OpenBrowserWindow();
    try {
      let prefFile = Services.prefs.getCharPref("addon.test.tabswitch.urlfile");
      if (prefFile) {
        testURLs = handleFile(win, prefFile);
      }
    } catch (ex) { /* error condition handled below */ }
    if (!testURLs || testURLs.length == 0) {
      dump("no tabs to test, 'addon.test.tabswitch.urlfile' pref isn't set to page set path\n");
      return;
    }

    yield waitForDelayedStartup(win);

    let gBrowser = win.gBrowser;

    // We don't want to catch scrolling the tabstrip in our tests
    gBrowser.tabContainer.style.visibility = "hidden";

    let initialTab = gBrowser.selectedTab;
    yield loadTabs(gBrowser, testURLs);

    // We'll switch back to about:blank after each tab switch
    // in an attempt to put the graphics layer into a "steady"
    // state before switching to the next tab.
    initialTab.linkedBrowser.loadURI("about:blank", null, null);

    let tabs = gBrowser.getTabsToTheEndFrom(initialTab);
    let times = [];

    for (let tab of tabs) {
      // Let's do an initial run to warm up any paint related caches
      // (like glyph caches for text). In the next loop we will start with
      // a GC before each switch so we don't need here.
      // Note: in case of multiple content processes, closing all the tabs
      // would close the related content processes, and even if we kept them
      // alive it would be unlikely that the same pages end up in the same
      // content processes, so we cannot do this at the manifest level.
      yield switchToTab(tab);
      yield switchToTab(initialTab);
    }

    for (let tab of tabs) {
      yield forceGC(win, tab.linkedBrowser);
      let time = yield switchToTab(tab);
      dump(`${tab.linkedBrowser.currentURI.spec}: ${time}ms\n`);
      times.push(time);
      yield switchToTab(initialTab);
    }

    let output = "<!DOCTYPE html>" +
                 '<html lang="en">' +
                 "<head><title>Tab Switch Results</title></head>" +
                 "<body><h1>Tab switch times</h1>" +
                 "<table>";
    let time = 0;
    for (let i in times) {
      time += times[i];
      output += "<tr><td>" + testURLs[i] + "</td><td>" + times[i] + "ms</td></tr>";
    }
    output += "</table></body></html>";
    dump("total tab switch time:" + time + "\n");

    let resultsTab = win.gBrowser.loadOneTab(
      "data:text/html;charset=utf-8," + encodeURIComponent(output), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    let pref = Services.prefs.getBoolPref("browser.tabs.warnOnCloseOtherTabs");
    if (pref)
      Services.prefs.setBoolPref("browser.tabs.warnOnCloseOtherTabs", false);
    win.gBrowser.removeAllTabsBut(resultsTab);
    if (pref)
      Services.prefs.setBoolPref("browser.tabs.warnOnCloseOtherTabs", pref);

    remotePage.sendAsyncMessage("tabswitch-test-results", {
      times,
      urls: testURLs,
    });

    win.close();
  });
}

function unloadFromWindow(window) {
  if (!window)
    return;
  let toolsMenu = window.document.getElementById("menu_ToolsPopup");
  if (!toolsMenu)
    return;
  toolsMenu.removeChild(window.document.getElementById("start_test_item"));
}

function loadIntoWindow(window) {
  if (!window)
    return;
  let item = window.document.createElement("menuitem");
  item.setAttribute("label", "Start test");
  item.id = "start_test_item";
  window.tab_switch_test = test;
  item.setAttribute("oncommand", "tab_switch_test(window)");
  let toolsMenu = window.document.getElementById("menu_ToolsPopup");
  if (!toolsMenu)
    return;
  toolsMenu.appendChild(item);
}

function install(aData, aReason) {}
function uninstall(aData, aReason) {}

function shutdown(aData, aReason) {
  // When the application is shutting down we normally don't have to clean
  // up any UI changes made
  if (aReason == APP_SHUTDOWN) {
    return;
  }

  Services.wm.removeListener(windowListener);

  // Unload from any existing windows
  let list = Services.wm.getEnumerator("navigator:browser");
  while (list.hasMoreElements()) {
    let window = list.getNext().QueryInterface(Ci.nsIDOMWindow);
    unloadFromWindow(window);
  }
  Services.obs.removeObserver(observer, "tabswitch-urlfile");

  remotePage.destroy();
}

function handleFile(win, file) {

  let localFile = Cc["@mozilla.org/file/local;1"]
    .createInstance(Ci.nsILocalFile);
  localFile.initWithPath(file);
  let localURI = Services.io.newFileURI(localFile);
  let req = new win.XMLHttpRequest();
  req.open("get", localURI.spec, false);
  req.send(null);


  let testURLs = [];
  let server = Services.prefs.getCharPref("addon.test.tabswitch.webserver");
  let maxurls = Services.prefs.getIntPref("addon.test.tabswitch.maxurls");
  let parent = server + "/tests/";
  let lines = req.responseText.split('<a href=\"');
  testURLs = [];
  if (maxurls && maxurls > 0) {
    lines.splice(maxurls, lines.length);
  }
  lines.forEach(function(a) {
    if (a.split('\"')[0] != "") {
      testURLs.push(parent + "tp5n/" + a.split('\"')[0]);
    }
  });

  return testURLs;
}

var observer = {
  observe(aSubject, aTopic, aData) {
    if (aTopic == "tabswitch-urlfile") {
      handleFile(aSubject, aData);
    }
  }
};

var remotePage;

function startup(aData, aReason) {
  // Load into any existing windows
  let list = Services.wm.getEnumerator("navigator:browser");
  let window;
  while (list.hasMoreElements()) {
    window = list.getNext().QueryInterface(Ci.nsIDOMWindow);
    loadIntoWindow(window);
  }

  // Load into any new windows
  Services.wm.addListener(windowListener);

  Services.obs.addObserver(observer, "tabswitch-urlfile");

  Services.ppmm.loadProcessScript("chrome://tabswitch/content/tabswitch-content-process.js", true);

  remotePage = new RemotePages("about:tabswitch");
  remotePage.addMessageListener("tabswitch-do-test", function doTest(msg) {
    test(msg.target.browser.ownerGlobal);
  });
}
