// Test that having two frames that request installs at the same time doesn't
// cause callback ID conflicts (discussed in bug 926712)

var gConcurrentTabs = [];
var gQueuedForInstall = [];
var gResults = [];

function frame_script() {
  /* globals addMessageListener, sendAsyncMessage*/
  addMessageListener("Test:StartInstall", () => {
    content.document.getElementById("installnow").click()
  });

  addEventListener("load", () => {
    sendAsyncMessage("Test:Loaded");

    content.addEventListener("InstallComplete", (e) => {
      sendAsyncMessage("Test:InstallComplete", e.detail);
    }, true);
  }, true);
}

var gAddonAndWindowListener = {
  onOpenWindow(win) {
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
  onCloseWindow(win) { },
  onInstallEnded(install) {
    install.cancel();
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediatorListener])
};

function installNext() {
  let tab = gQueuedForInstall.shift();
  tab.linkedBrowser.messageManager.sendAsyncMessage("Test:StartInstall");
}

function createTab(url) {
  let tab = gBrowser.addTab(url);
  tab.linkedBrowser.messageManager.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);

  tab.linkedBrowser.messageManager.addMessageListener("Test:InstallComplete", ({data}) => {
    gResults.push(data);
    if (gResults.length == 2) {
      executeSoon(endThisTest);
    }
  });

  return tab;
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIRESECUREORIGIN, false);
  Services.wm.addListener(gAddonAndWindowListener);
  AddonManager.addInstallListener(gAddonAndWindowListener);
  registerCleanupFunction(function() {
    Services.wm.removeListener(gAddonAndWindowListener);
    AddonManager.removeInstallListener(gAddonAndWindowListener);
    Services.prefs.clearUserPref(PREF_LOGGING_ENABLED);
    Services.prefs.clearUserPref(PREF_INSTALL_REQUIRESECUREORIGIN);

    Services.perms.remove(makeURI("http://example.com"), "install");
    Services.perms.remove(makeURI("http://example.org"), "install");

    while (gConcurrentTabs.length) {
      gBrowser.removeTab(gConcurrentTabs.shift());
    }
  });

  let pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  gConcurrentTabs.push(createTab(TESTROOT + "concurrent_installs.html"));
  gConcurrentTabs.push(createTab(TESTROOT2 + "concurrent_installs.html"));

  let promises = gConcurrentTabs.map((t) => {
    return new Promise(resolve => {
      t.linkedBrowser.messageManager.addMessageListener("Test:Loaded", resolve);
    });
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
    if (loc.includes("example.org")) {
      ok(xpi.includes("example.org"), "Should get .org XPI for .org loc");
    } else if (loc.includes("example.com")) {
      ok(xpi.includes("example.com"), "Should get .com XPI for .com loc");
    } else {
      ok(false, "Should never get anything that isn't from example.org or example.com");
    }
  }

  finish();
}

