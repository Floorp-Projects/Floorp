/**
 * Tests that chromeFlags are set properly on windows that are
 * being opened from content.
 */

// The following features set chrome flags on new windows and are
// supported by web content. The schema for each property on this
// object is as follows:
//
// <feature string>: {
//   flag: <associated nsIWebBrowserChrome flag>,
//   defaults_to: <what this feature defaults to normally>
// }
const ALLOWED = {
  "toolbar": {
    flag: Ci.nsIWebBrowserChrome.CHROME_TOOLBAR,
    defaults_to: true,
  },
  "personalbar": {
    flag: Ci.nsIWebBrowserChrome.CHROME_PERSONAL_TOOLBAR,
    defaults_to: true,
  },
  "menubar": {
    flag: Ci.nsIWebBrowserChrome.CHROME_MENUBAR,
    defaults_to: true,
  },
  "scrollbars": {
    flag: Ci.nsIWebBrowserChrome.CHROME_SCROLLBARS,
    defaults_to: false,
  },
  "minimizable": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_MIN,
    defaults_to: true,
  },
};

// Construct a features string that flips all ALLOWED features
// to not be their defaults.
const ALLOWED_STRING = Object.keys(ALLOWED).map(feature => {
  let toValue = ALLOWED[feature].defaults_to ? "no" : "yes";
  return `${feature}=${toValue}`;
}).join(",");

// The following are not allowed from web content, at least
// in the default case (since some are disabled by default
// via the dom.disable_window_open_feature pref branch).
const DISALLOWED = {
  "location": {
    flag: Ci.nsIWebBrowserChrome.CHROME_LOCATIONBAR,
    defaults_to: true,
  },
  "chrome": {
    flag: Ci.nsIWebBrowserChrome.CHROME_OPENAS_CHROME,
    defaults_to: false,
  },
  "dialog": {
    flag: Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG,
    defaults_to: false,
  },
  "private": {
    flag: Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    defaults_to: false,
  },
  "non-private": {
    flag: Ci.nsIWebBrowserChrome.CHROME_NON_PRIVATE_WINDOW,
    defaults_to: false,
  },
  // "all":
  //   checked manually, since this is an aggregate
  //   flag.
  //
  // "remote":
  //   checked manually, since its default value will
  //   depend on whether or not e10s is enabled by default.
  "popup": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_POPUP,
    defaults_to: false,
  },
  "alwaysLowered": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_LOWERED,
    defaults_to: false,
  },
  "z-lock": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_LOWERED, // Renamed to alwaysLowered
    defaults_to: false,
  },
  "alwaysRaised": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_RAISED,
    defaults_to: false,
  },
  "macsuppressanimation": {
    flag: Ci.nsIWebBrowserChrome.CHROME_MAC_SUPPRESS_ANIMATION,
    defaults_to: false,
  },
  "extrachrome": {
    flag: Ci.nsIWebBrowserChrome.CHROME_EXTRA,
    defaults_to: false,
  },
  "centerscreen": {
    flag: Ci.nsIWebBrowserChrome.CHROME_CENTER_SCREEN,
    defaults_to: false,
  },
  "dependent": {
    flag: Ci.nsIWebBrowserChrome.CHROME_DEPENDENT,
    defaults_to: false,
  },
  "modal": {
    flag: Ci.nsIWebBrowserChrome.CHROME_MODAL,
    defaults_to: false,
  },
  "titlebar": {
    flag: Ci.nsIWebBrowserChrome.CHROME_TITLEBAR,
    defaults_to: true,
  },
  "close": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_CLOSE,
    defaults_to: true,
  },
  "resizable": {
    flag: Ci.nsIWebBrowserChrome.CHROME_WINDOW_RESIZE,
    defaults_to: true,
  },
  "status": {
    flag: Ci.nsIWebBrowserChrome.CHROME_STATUSBAR,
    defaults_to: true,
  },
};

// Construct a features string that flips all DISALLOWED features
// to not be their defaults.
const DISALLOWED_STRING = Object.keys(DISALLOWED).map(feature => {
  let toValue = DISALLOWED[feature].defaults_to ? "no" : "yes";
  return `${feature}=${toValue}`;
}).join(",");

const FEATURES = [ALLOWED_STRING, DISALLOWED_STRING].join(",");

const SCRIPT_PAGE = `data:text/html,<script>window.open("about:blank", "_blank", "${FEATURES}");</script>`;
const SCRIPT_PAGE_FOR_CHROME_ALL = `data:text/html,<script>window.open("about:blank", "_blank", "all");</script>`;

// This magic value of 2 means that by default, when content tries
// to open a new window, it'll actually open in a new window instead
// of a new tab.
Services.prefs.setIntPref("browser.link.open_newwindow", 2);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.link.open_newwindow");
});

/**
 * Given some nsIDOMWindow for a window running in the parent
 * process, return the nsIWebBrowserChrome chrome flags for
 * the associated XUL window.
 *
 * @param win (nsIDOMWindow)
 *        Some window in the parent process.
 * @returns int
 */
function getParentChromeFlags(win) {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebNavigation)
            .QueryInterface(Ci.nsIDocShellTreeItem)
            .treeOwner
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIXULWindow)
            .chromeFlags;
}

/**
 * For some chromeFlags, ensures that flags that are in the
 * ALLOWED group were modified, and that flags in the DISALLOWED
 * group were not modified.
 *
 * @param chromeFlags (int)
 *        Some chromeFlags to check.
 */
function assertContentFlags(chromeFlags) {
  for (let feature in ALLOWED) {
    let flag = ALLOWED[feature].flag;

    if (ALLOWED[feature].defaults_to) {
      // The feature is supposed to default to true, so we should
      // have been able to flip it off.
      Assert.ok(!(chromeFlags & flag),
                `Expected feature ${feature} to be disabled`);
    } else {
      // The feature is supposed to default to false, so we should
      // have been able to flip it on.
      Assert.ok((chromeFlags & flag),
                `Expected feature ${feature} to be enabled`);
    }
  }

  for (let feature in DISALLOWED) {
    let flag = DISALLOWED[feature].flag;
    if (DISALLOWED[feature].defaults_to) {
      // The feature is supposed to default to true, so it should
      // stay true.
      Assert.ok((chromeFlags & flag),
                `Expected feature ${feature} to be unchanged`);
    } else {
      // The feature is supposed to default to false, so it should
      // stay false.
      Assert.ok(!(chromeFlags & flag),
                `Expected feature ${feature} to be unchanged`);
    }
  }
}

/**
 * Opens a window from content using window.open with the
 * features computed from ALLOWED and DISALLOWED. The computed
 * feature string attempts to flip every feature away from their
 * default.
 */
add_task(function* test_new_remote_window_flags() {
  let newWinPromise = BrowserTestUtils.waitForNewWindow();

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: SCRIPT_PAGE,
  }, function*(browser) {
    let win = yield newWinPromise;
    let parentChromeFlags = getParentChromeFlags(win);
    assertContentFlags(parentChromeFlags);

    if (win.gMultiProcessBrowser) {
      Assert.ok(parentChromeFlags &
                Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW,
                "Should be remote by default");
    } else {
      Assert.ok(!(parentChromeFlags &
                  Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW),
                "Should not be remote by default");
    }

    // Confusingly, chromeFlags also exist in the content process
    // as part of the TabChild, so we have to check those too.
    let b = win.gBrowser.selectedBrowser;
    let contentChromeFlags = yield ContentTask.spawn(b, null, function*() {
      docShell.QueryInterface(Ci.nsIInterfaceRequestor);
      try {
        // This will throw if we're not a remote browser.
        return docShell.getInterface(Ci.nsITabChild)
                       .QueryInterface(Ci.nsIWebBrowserChrome)
                       .chromeFlags;
      } catch (e) {
        // This must be a non-remote browser...
        return docShell.QueryInterface(Ci.nsIDocShellTreeItem)
                       .treeOwner
                       .QueryInterface(Ci.nsIWebBrowserChrome)
                       .chromeFlags;
      }
    });

    assertContentFlags(contentChromeFlags);
    Assert.ok(!(contentChromeFlags &
                Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW),
              "Should not be remote in the content process.");

    yield BrowserTestUtils.closeWindow(win);
  });

  // We check "all" manually, since that's an aggregate flag
  // and doesn't fit nicely into the ALLOWED / DISALLOWED scheme
  newWinPromise = BrowserTestUtils.waitForNewWindow();

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: SCRIPT_PAGE_FOR_CHROME_ALL,
  }, function*(browser) {
    let win = yield newWinPromise;
    let parentChromeFlags = getParentChromeFlags(win);
    Assert.notEqual((parentChromeFlags & Ci.nsIWebBrowserChrome.CHROME_ALL),
                    Ci.nsIWebBrowserChrome.CHROME_ALL,
                    "Should not have been able to set CHROME_ALL");
    yield BrowserTestUtils.closeWindow(win);
  });
});
