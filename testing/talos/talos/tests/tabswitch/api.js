// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-

/* globals ExtensionAPI, Services */

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

let TalosParentProfiler;

/**
 * Returns a Promise that resolves when browser-delayed-startup-finished
 * fires for a given window
 *
 * @param win
 *        The window that we're waiting for the notification for.
 * @returns Promise
 */
function waitForDelayedStartup(win) {
  return new Promise(resolve => {
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
  return new Promise(resolve => {
    gBrowser.loadTabs(urls, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    let waitingToLoad = new Set(urls);

    let listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
      onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
        let loadedState =
          Ci.nsIWebProgressListener.STATE_STOP |
          Ci.nsIWebProgressListener.STATE_IS_NETWORK;
        if (
          (aStateFlags & loadedState) == loadedState &&
          !aWebProgress.isLoadingDocument &&
          aWebProgress.isTopLevel &&
          Components.isSuccessCode(aStatus)
        ) {
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
 * For some <xul:tab> in a browser window, have that window switch
 * to that tab. Returns a Promise that resolves ones the tab content
 * has been presented to the user.
 */
async function switchToTab(tab) {
  let browser = tab.linkedBrowser;
  let gBrowser = tab.ownerGlobal.gBrowser;

  let start = Cu.now();

  // We need to wait for the TabSwitchDone event to make sure
  // that the async tab switcher has shut itself down.
  let switchDone = waitForTabSwitchDone(browser);
  // Set up our promise that will wait for the content to be
  // presented.
  let finishPromise = waitForContentPresented(browser);
  // Finally, do the tab switch.
  gBrowser.selectedTab = tab;

  await switchDone;
  let finish = await finishPromise;

  return finish - start;
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
  return new Promise(resolve => {
    let gBrowser = browser.ownerGlobal.gBrowser;
    gBrowser.addEventListener(
      "TabSwitchDone",
      function onTabSwitchDone() {
        resolve();
      },
      { once: true }
    );
  });
}

/**
 * For some <xul:browser>, returns a Promise that resolves once its
 * content has been presented to the user.
 *
 * @param browser (<xul:browser>)
 *        The browser we expect to be presented.
 *
 * @returns Promise
 *        Resolves once the content has been presented. Resolves to
 *        the system time that the presentation occurred at, in
 *        milliseconds since midnight 01 January, 1970 UTC.
 */
function waitForContentPresented(browser) {
  return new Promise(resolve => {
    browser.addEventListener(
      "MozLayerTreeReady",
      function onLayersReady(event) {
        let now = Cu.now();
        TalosParentProfiler.mark("MozLayerTreeReady seen by tabswitch");
        resolve(now);
      },
      { once: true }
    );
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
  browser.messageManager.loadFrameScript(
    "chrome://pageloader/content/talos-content.js",
    false
  );

  win.windowUtils.garbageCollect();

  return new Promise(resolve => {
    let mm = browser.messageManager;
    mm.addMessageListener("Talos:ForceGC:OK", function onTalosContentForceGC(
      msg
    ) {
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
async function test(window) {
  if (!window.gMultiProcessBrowser) {
    dump(
      "** The tabswitch Talos test does not support running in non-e10s mode " +
        "anymore! Bailing out!\n"
    );
    return;
  }

  TalosParentProfiler = ChromeUtils.import(
    "resource://talos-powers/TalosParentProfiler.jsm"
  ).TalosParentProfiler;

  let testURLs = [];

  let win = window.OpenBrowserWindow();
  try {
    let prefFile = Services.prefs.getCharPref("addon.test.tabswitch.urlfile");
    if (prefFile) {
      testURLs = handleFile(win, prefFile);
    }
  } catch (ex) {
    /* error condition handled below */
  }
  if (!testURLs || !testURLs.length) {
    dump(
      "no tabs to test, 'addon.test.tabswitch.urlfile' pref isn't set to page set path\n"
    );
    return;
  }

  await waitForDelayedStartup(win);

  let gBrowser = win.gBrowser;

  // We don't want to catch scrolling the tabstrip in our tests
  gBrowser.tabContainer.style.opacity = "0";

  let initialTab = gBrowser.selectedTab;
  await loadTabs(gBrowser, testURLs);

  // We'll switch back to about:blank after each tab switch
  // in an attempt to put the graphics layer into a "steady"
  // state before switching to the next tab.
  initialTab.linkedBrowser.loadURI("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
  });

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
    await switchToTab(tab);
    await switchToTab(initialTab);
  }

  for (let tab of tabs) {
    // Moving a tab causes expensive style/layout computations on the tab bar
    // that are delayed using requestAnimationFrame, so wait for an animation
    // frame callback + one tick to ensure we aren't measuring the time it
    // takes to move a tab.
    gBrowser.moveTabTo(tab, 1);
    await new Promise(resolve => win.requestAnimationFrame(resolve));
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

    await forceGC(win, tab.linkedBrowser);
    TalosParentProfiler.resume();
    let time = await switchToTab(tab);
    TalosParentProfiler.pause(
      "TabSwitch Test: " + tab.linkedBrowser.currentURI.spec
    );
    dump(`${tab.linkedBrowser.currentURI.spec}: ${time}ms\n`);
    times.push(time);
    await switchToTab(initialTab);
  }

  let output =
    "<!DOCTYPE html>" +
    '<html lang="en">' +
    "<head><title>Tab Switch Results</title></head>" +
    "<body><h1>Tab switch times</h1>" +
    "<table>";
  let time = 0;
  for (let i in times) {
    time += times[i];
    output +=
      "<tr><td>" + testURLs[i] + "</td><td>" + times[i] + "ms</td></tr>";
  }
  output += "</table></body></html>";
  dump("total tab switch time:" + time + "\n");

  let resultsTab = win.gBrowser.loadOneTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(output),
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    }
  );

  win.gBrowser.selectedTab = resultsTab;

  remotePage.sendAsyncMessage("tabswitch-test-results", {
    times,
    urls: testURLs,
  });

  TalosParentProfiler.afterProfileGathered().then(() => {
    win.close();
  });
}

// This just has to match up with the make_talos_domain function in talos.py
function makeTalosDomain(host) {
  return host + "-talos";
}

function handleFile(win, file) {
  let localFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  localFile.initWithPath(file);
  let localURI = Services.io.newFileURI(localFile);
  let req = new win.XMLHttpRequest();
  req.open("get", localURI.spec, false);
  req.send(null);

  let testURLs = [];
  let maxurls = Services.prefs.getIntPref("addon.test.tabswitch.maxurls");
  let lines = req.responseText.split('<a href="');
  testURLs = [];
  if (maxurls && maxurls > 0) {
    lines.splice(maxurls, lines.length);
  }
  lines.forEach(function(a) {
    let url = a.split('"')[0];
    if (url != "") {
      let domain = url.split("/")[0];
      if (domain != "") {
        testURLs.push(`http://${makeTalosDomain(domain)}/fis/tp5n/${url}`);
      }
    }
  });

  return testURLs;
}

var remotePage;

this.tabswitch = class extends ExtensionAPI {
  getAPI(context) {
    return {
      tabswitch: {
        setup({ processScriptPath }) {
          AboutNewTab.newTabURL = "about:blank";

          const processScriptURL = context.extension.baseURI.resolve(
            processScriptPath
          );
          Services.ppmm.loadProcessScript(processScriptURL, true);
          remotePage = new RemotePages("about:tabswitch");
          remotePage.addMessageListener("tabswitch-do-test", function doTest(
            msg
          ) {
            test(msg.target.browser.ownerGlobal);
          });

          return () => {
            Services.ppmm.sendAsyncMessage("Tabswitch:Teardown");
            remotePage.destroy();
            AboutNewTab.resetNewTabURL();
          };
        },
      },
    };
  }
};
