// Test that having two frames that request installs at the same time doesn't
// cause callback ID conflicts (discussed in bug 926712)

let {Promise} = Cu.import("resource://gre/modules/Promise.jsm");

let gConcurrentTabs = [];
let gQueuedForInstall = [];
let gResults = [];

let gAddonAndWindowListener = {
  onOpenWindow: function(win) {
    var window = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    info("Window opened");

    waitForFocus(function() {
      info("Focused!");
      // Initially the accept button is disabled on a countdown timer
      let button = window.document.documentElement.getButton("accept");
      button.disabled = false;
      if (gQueuedForInstall.length > 0) {
        // Start downloading the next add-on while we accept this dialog:
        installNext();
      }
      window.document.documentElement.acceptDialog();
    }, window);
  },
  onCloseWindow: function(win) { },
  onInstallEnded: function(install) {
    install.cancel();
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediatorListener])
};

function installNext() {
  let tab = gQueuedForInstall.shift();
  tab.linkedBrowser.contentDocument.getElementById("installnow").click();
}

function winForTab(t) {
  return t.linkedBrowser.contentDocument.defaultView;
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);
  Services.wm.addListener(gAddonAndWindowListener);
  AddonManager.addInstallListener(gAddonAndWindowListener);
  registerCleanupFunction(function() {
    Services.wm.removeListener(gAddonAndWindowListener);
    AddonManager.removeInstallListener(gAddonAndWindowListener);
    Services.prefs.clearUserPref(PREF_LOGGING_ENABLED);

    Services.perms.remove("example.com", "install");
    Services.perms.remove("example.org", "install");

    while (gConcurrentTabs.length) {
      gBrowser.removeTab(gConcurrentTabs.shift());
    }
  });

  let pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  gConcurrentTabs.push(gBrowser.addTab(TESTROOT + "concurrent_installs.html"));
  gConcurrentTabs.push(gBrowser.addTab(TESTROOT2 + "concurrent_installs.html"));

  let promises = gConcurrentTabs.map((t) => {
    let deferred = Promise.defer();
    t.linkedBrowser.addEventListener("load", () => {
      let win = winForTab(t);
      if (win.location.host.startsWith("example")) {
        win.wrappedJSObject.installTriggerCallback = function(rv) {
          gResults.push(rv);
          if (gResults.length == 2) {
            executeSoon(endThisTest);
          }
        };
        deferred.resolve();
      }
    }, true);
    return deferred.promise;
  });

  Promise.all(promises).then(() => {
    gQueuedForInstall = [...gConcurrentTabs];
    installNext();
  });
}

function endThisTest() {
  is(gResults.length, 2, "Should have two urls");
  isnot(gResults[0].loc, gResults[1].loc, "Should not have results from the same page.");
  isnot(gResults[0].xpi, gResults[1].xpi, "Should not have the same XPIs.");
  for (let i = 0; i < 2; i++) {
    let {loc, xpi} = gResults[i];
    if (loc.contains("example.org")) {
      ok(xpi.contains("example.org"), "Should get .org XPI for .org loc");
    } else if (loc.contains("example.com")) {
      ok(xpi.contains("example.com"), "Should get .com XPI for .com loc");
    } else {
      ok(false, "Should never get anything that isn't from example.org or example.com");
    }
  }

  finish();
}

