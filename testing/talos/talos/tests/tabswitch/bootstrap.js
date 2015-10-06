
// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource:///modules/NewTabURL.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var aboutBlankTab = null;
var Profiler = null;

var windowListener = {
  onOpenWindow: function(aWindow) {
    // Ensure we don't get tiles which contact the network
    NewTabURL.override("about:blank")

    // Wait for the window to finish loading
    let window = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowInternal || Ci.nsIDOMWindow);
    let cb = function() {
      window.removeEventListener("load", cb, false);
      loadIntoWindow(window);
    };
    window.addEventListener("load", cb, false);
  },

  onCloseWindow: function(aWindow) {
    NewTabURL.reset()
  },

  onWindowTitleChange: function(aWindow, aTitle) {
  }
};

function promiseOneEvent(target, eventName, capture) {
  let deferred = Promise.defer();
  target.addEventListener(eventName, function handler(event) {
    target.removeEventListener(eventName, handler, capture);
    deferred.resolve();
  }, capture);
  return deferred.promise;
}

function executeSoon(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

function whenDelayedStartupFinished(win, callback) {
  const topic = "browser-delayed-startup-finished";
  Services.obs.addObserver(function onStartup(subject) {
    if (win == subject) {
      Services.obs.removeObserver(onStartup, topic);
      executeSoon(callback);
    }
  }, topic, false);
}

function waitForTabLoads(browser, urls, callback) {
  // Make sure we get load events for all the urls we need.
  // Before we kept a count and sometimes received load events for other tabs
  // and got a bad count.
  var waitingToLoad = {};
  for (var i = 0; i < urls.length; i++) {
    waitingToLoad[urls[i]] = true;
  }
  let listener = {
    QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
                                           "nsISupportsWeakReference"]),

    onStateChange: function(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      let loadedState = Ci.nsIWebProgressListener.STATE_STOP |
        Ci.nsIWebProgressListener.STATE_IS_NETWORK;
      if ((aStateFlags & loadedState) == loadedState &&
          !aWebProgress.isLoadingDocument &&
          aWebProgress.DOMWindow == aWebProgress.DOMWindow.top) {
        delete waitingToLoad[aBrowser.currentURI.spec];
        dump("Loaded: " + aBrowser.currentURI.spec + "\n");

        aBrowser.messageManager.loadFrameScript("chrome://pageloader/content/talos-content.js", false);
        if (Object.keys(waitingToLoad).length == 0) {
          browser.removeTabsProgressListener(listener);
          callback();
        }
      }
    },

    onLocationChange: function(aProgress, aRequest, aURI) {
    },
    onProgressChange: function(aWebProgress, aRequest, curSelf, maxSelf, curTot, maxTot) {},
    onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {},
    onSecurityChange: function(aWebProgress, aRequest, aState) {}
  }
  browser.addTabsProgressListener(listener);
}

function loadTabs(urls, win, callback) {
  let context = {};
  Services.scriptloader.loadSubScript("chrome://pageloader/content/Profiler.js", context);
  Profiler = context.Profiler;

  // We don't want to catch scrolling the tabstrip in our tests
  win.gBrowser.tabContainer.style.visibility = "hidden";

  let initialTab = win.gBrowser.selectedTab;
  // Set about:blank to be the first tab. This will allow us to use about:blank
  // to let paint event stabilize and make all tab switch more even.
  initialTab.linkedBrowser.loadURI("about:blank", null, null);
  waitForTabLoads(win.gBrowser, urls, function() {
    let tabs = win.gBrowser.getTabsToTheEndFrom(initialTab);
    callback(tabs);
  });
  win.gBrowser.loadTabs(urls, true);

  aboutBlankTab = initialTab;
}

function waitForTabSwitchDone(win, callback) {
  if (win.gBrowser.selectedBrowser.isRemoteBrowser) {
    var list = function onSwitch() {
      win.gBrowser.removeEventListener("TabSwitched", list);
      callback();
    };

    win.gBrowser.addEventListener("TabSwitched", list);

  } else {
    // Tab switch is sync so it has already happened.
    callback();
  }
}

function runTest(tabs, win, callback) {
  let startTab = win.gBrowser.selectedTab;
  let times = [];
  runTestHelper(startTab, tabs, 0, win, times, function() {
    callback(times);
  });
}

function runTestHelper(startTab, tabs, index, win, times, callback) {

  let tab = tabs[index];

  // Clean up garbage so GC/CC doesn't randomly occur during our test
  // making the results noisy
  win.QueryInterface(Ci.nsIInterfaceRequestor)
     .getInterface(Ci.nsIDOMWindowUtils)
     .garbageCollect();

  forceContentGC(tab.linkedBrowser).then(function() {
    if (typeof(Profiler) !== "undefined") {
      Profiler.resume(tab.linkedBrowser.currentURI.spec);
    }
    let start = win.performance.now();
    win.gBrowser.selectedTab = tab;

    waitForTabSwitchDone(win, function() {
      // This will fire when we're about to paint the tab switch
      win.requestAnimationFrame(function() {
        // This will fire on the next vsync tick after the tab has switched.
        // If we have a sync transaction on the compositor, that time will
        // be included here. It will not accuratly capture the composite time
        // or the time of async transaction.
        // XXX: This will need to be adjusted for e10s since we need to block
        //      on the child/content having painted.
        win.requestAnimationFrame(function() {
          times.push(win.performance.now() - start);
          if (typeof(Profiler) !== "undefined") {
            Profiler.pause(tab.linkedBrowser.currentURI.spec);
          }

          // Select about:blank which will let the browser reach a steady no
          // painting state
          win.gBrowser.selectedTab = aboutBlankTab;

          win.requestAnimationFrame(function() {
            win.requestAnimationFrame(function() {
              if (index == tabs.length - 1) {
                callback();
              } else {
                runTestHelper(startTab, tabs, index + 1, win, times, function() {
                  callback();
                });
              }
            });
          });
        });
      });
    });
  });
}

function forceContentGC(browser) {
  return new Promise((resolve) => {
    let mm = browser.messageManager;
    mm.addMessageListener("Talos:ForceGC:OK", function onTalosContentForceGC(msg) {
      mm.removeMessageListener("Talos:ForceGC:OK", onTalosContentForceGC);
      resolve();
    });
    mm.sendAsyncMessage("Talos:ForceGC");
  });
}

function test(window) {
  let win = window.OpenBrowserWindow();
  let testURLs;

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
  whenDelayedStartupFinished(win, function() {
    loadTabs(testURLs, win, function(tabs) {
      runTest(tabs, win, function(times) {
        let output = '<!DOCTYPE html>'+
                     '<html lang="en">'+
                     '<head><title>Tab Switch Results</title></head>'+
                     '<body><h1>Tab switch times</h1>' +
                     '<table>';
        let time = 0;
        for(let i in times) {
          time += times[i];
          output += '<tr><td>' + testURLs[i] + '</td><td>' + times[i] + 'ms</td></tr>';
        }
        output += '</table></body></html>';
        dump("total tab switch time:" + time + "\n");

        let resultsTab = win.gBrowser.loadOneTab('data:text/html;charset=utf-8,' +
                                                 encodeURIComponent(output));
        let pref = Services.prefs.getBoolPref("browser.tabs.warnOnCloseOtherTabs");
        if (pref)
          Services.prefs.setBoolPref("browser.tabs.warnOnCloseOtherTabs", false);
        win.gBrowser.removeAllTabsBut(resultsTab);
        if (pref)
          Services.prefs.setBoolPref("browser.tabs.warnOnCloseOtherTabs", pref);
        Services.obs.notifyObservers(win, 'tabswitch-test-results', JSON.stringify({'times': times, 'urls': testURLs}));
      });
    });
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
  Services.obs.removeObserver(observer, "tabswitch-do-test");
}

function handleFile(win, file) {

  let localFile = Cc["@mozilla.org/file/local;1"]
    .createInstance(Ci.nsILocalFile);
  localFile.initWithPath(file);
  let localURI = Services.io.newFileURI(localFile);
  let req = new win.XMLHttpRequest();
  req.open('get', localURI.spec, false);
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
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "tabswitch-do-test") {
      test(aSubject);
    } else if (aTopic == "tabswitch-urlfile") {
      handleFile(aSubject, aData);
    }
  }
};

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

  Services.obs.addObserver(observer, "tabswitch-urlfile", false);
  Services.obs.addObserver(observer, "tabswitch-do-test", false);
}
