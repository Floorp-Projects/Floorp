"use strict";

requestLongerTimeout(5);

/**
 * Opens windows from content using window.open with several features patterns.
 */
add_task(async function test_popup_conditions() {
  const PATTERNS = [
    { features: "", popup: false },

    // If features isn't empty, the following should be true to open tab/window:
    //   * location or toolbar (defaults to false)
    //   * menubar (defaults to false)
    //   * resizable (defaults to true)
    //   * scrollbars (defaults to false)
    //   * status (defaults to false)
    // and also the following shouldn't be specified:
    //   * left or screenX
    //   * top or screenY
    //   * width or innerWidth
    //   * height or innerHeight
    { features: "location,menubar,resizable,scrollbars,status", popup: false },
    { features: "toolbar,menubar,resizable,scrollbars,status", popup: false },
    {
      features: "location,menubar,toolbar,resizable,scrollbars,status",
      popup: false,
    },

    // resizable defaults to true.
    { features: "location,menubar,scrollbars,status", popup: false },

    // The following testcases use "location,menubar,scrollbars,status"
    // as the base non-popup case, and test the boundary between popup
    // vs non-popup.

    // If either location or toolbar is true, not popup.
    {
      features: "location=0,toolbar,menubar,resizable,scrollbars,status",
      popup: false,
    },
    {
      features: "location,toolbar=0,menubar,resizable,scrollbars,status",
      popup: false,
    },
    {
      features: "location,toolbar,menubar,resizable,scrollbars,status",
      popup: false,
    },

    // If both location and toolbar are false, popup.
    {
      features: "location=0,toolbar=0,menubar,resizable,scrollbars,status",
      popup: true,
    },
    { features: "menubar,scrollbars,status", popup: true },

    // If menubar is false, popup.
    { features: "location,resizable,scrollbars,status", popup: true },
    { features: "location,menubar=0,resizable,scrollbars,status", popup: true },

    // If resizable is true, not popup.
    {
      features: "location,menubar,resizable=yes,scrollbars,status",
      popup: false,
    },

    // If resizable is false, popup.
    { features: "location,menubar,resizable=0,scrollbars,status", popup: true },

    // If scrollbars is false, popup.
    { features: "location,menubar,resizable,status", popup: true },
    { features: "location,menubar,resizable,scrollbars=0,status", popup: true },

    // If status is false, popup.
    { features: "location,menubar,resizable,scrollbars", popup: true },
    { features: "location,menubar,resizable,scrollbars,status=0", popup: true },

    // If either width or innerWidth is specified, popup.
    { features: "location,menubar,scrollbars,status,width=100", popup: true },
    {
      features: "location,menubar,scrollbars,status,innerWidth=100",
      popup: true,
    },

    // outerWidth has no effect.
    {
      features: "location,menubar,scrollbars,status,outerWidth=100",
      popup: false,
    },

    // left or screenX alone doesn't make it a popup.
    {
      features: "location,menubar,scrollbars,status,left=100",
      popup: false,
    },
    {
      features: "location,menubar,scrollbars,status,screenX=100",
      popup: false,
    },

    // top or screenY alone doesn't make it a popup.
    {
      features: "location,menubar,scrollbars,status,top=100",
      popup: false,
    },
    {
      features: "location,menubar,scrollbars,status,screenY=100",
      popup: false,
    },

    // height or innerHeight alone doesn't make it a popup.
    {
      features: "location,menubar,scrollbars,status,height=100",
      popup: false,
    },
    {
      features: "location,menubar,scrollbars,status,innerHeight=100",
      popup: false,
    },
    // outerHeight has no effect.
    {
      features: "location,menubar,scrollbars,status,outerHeight=100",
      popup: false,
    },

    // Most feature defaults to false if the feature is not empty.
    // Specifying only some of them opens a popup.
    { features: "location", popup: true },
    { features: "toolbar", popup: true },
    { features: "location,toolbar", popup: true },
    { features: "menubar", popup: true },
    { features: "location,toolbar,menubar", popup: true },
    { features: "resizable", popup: true },
    { features: "scrollbars", popup: true },
    { features: "status", popup: true },
    { features: "left=500", popup: true },
    { features: "top=500", popup: true },
    { features: "left=500,top=500", popup: true },
    { features: "width=500", popup: true },
    { features: "height=500", popup: true },
    { features: "width=500, height=500", popup: true },

    // Specifying unknown feature makes the feature not empty.
    { features: "someunknownfeature", popup: true },

    // personalbar have no effect.
    { features: "personalbar=0", popup: true },
    { features: "personalbar=yes", popup: true },
    {
      features: "location,menubar,resizable,scrollbars,status,personalbar=0",
      popup: false,
    },
    {
      features: "location,menubar,resizable,scrollbars,status,personalbar=yes",
      popup: false,
    },

    // noopener is removed before testing if feature is empty.
    { features: "noopener", popup: false },

    // noreferrer is removed before testing if feature is empty.
    { features: "noreferrer", popup: false },
  ];

  const WINDOW_FLAGS = {
    CHROME_WINDOW_BORDERS: true,
    CHROME_WINDOW_CLOSE: true,
    CHROME_WINDOW_RESIZE: true,
    CHROME_LOCATIONBAR: true,
    CHROME_STATUSBAR: true,
    CHROME_SCROLLBARS: true,
    CHROME_TITLEBAR: true,

    CHROME_MENUBAR: true,
    CHROME_TOOLBAR: true,
    CHROME_PERSONAL_TOOLBAR: true,
  };

  const POPUP_FLAGS = {
    CHROME_WINDOW_BORDERS: true,
    CHROME_WINDOW_CLOSE: true,
    CHROME_WINDOW_RESIZE: true,
    CHROME_LOCATIONBAR: true,
    CHROME_STATUSBAR: true,
    CHROME_SCROLLBARS: true,
    CHROME_TITLEBAR: true,

    CHROME_MENUBAR: false,
    CHROME_TOOLBAR: false,
    CHROME_PERSONAL_TOOLBAR: false,
  };

  async function test_patterns({ nonPopup }) {
    for (const { features, popup } of PATTERNS) {
      const BLANK_PAGE = "data:text/html,";
      const OPEN_PAGE = "data:text/plain,hello";
      const SCRIPT_PAGE = `data:text/html,<script>window.open("${OPEN_PAGE}", "", "${features}");</script>`;

      async function testNewWindow(flags) {
        await BrowserTestUtils.withNewTab(
          {
            gBrowser,
            url: BLANK_PAGE,
          },
          async function(browser) {
            const newWinPromise = BrowserTestUtils.waitForNewWindow();
            await BrowserTestUtils.loadURI(gBrowser, SCRIPT_PAGE);

            const win = await newWinPromise;
            const parentChromeFlags = getParentChromeFlags(win);

            for (const [name, visible] of Object.entries(flags)) {
              if (visible) {
                Assert.equal(
                  !!(parentChromeFlags & Ci.nsIWebBrowserChrome[name]),
                  true,
                  `${name} should be present for features "${features}"`
                );
              } else {
                Assert.equal(
                  !!(parentChromeFlags & Ci.nsIWebBrowserChrome[name]),
                  false,
                  `${name} should not be present for features "${features}"`
                );
              }
            }

            await BrowserTestUtils.closeWindow(win);
          }
        );
      }

      async function testNewTab() {
        await BrowserTestUtils.withNewTab(
          {
            gBrowser,
            url: BLANK_PAGE,
          },
          async function(browser) {
            const newTabPromise = BrowserTestUtils.waitForNewTab(
              gBrowser,
              OPEN_PAGE
            );
            await BrowserTestUtils.loadURI(gBrowser, SCRIPT_PAGE);

            let tab = await newTabPromise;
            BrowserTestUtils.removeTab(tab);
          }
        );
      }

      async function testCurrentTab() {
        await BrowserTestUtils.withNewTab(
          {
            gBrowser,
            url: BLANK_PAGE,
          },
          async function(browser) {
            const pagePromise = BrowserTestUtils.browserLoaded(
              browser,
              false,
              OPEN_PAGE
            );
            await BrowserTestUtils.loadURI(gBrowser, SCRIPT_PAGE);

            await pagePromise;
          }
        );
      }

      if (!popup) {
        if (nonPopup == "window") {
          await testNewWindow(WINDOW_FLAGS);
        } else if (nonPopup == "tab") {
          await testNewTab();
        } else {
          // current tab
          await testCurrentTab();
        }
      } else {
        await testNewWindow(POPUP_FLAGS);
      }
    }
  }

  // Non-popup is opened in a new tab (default behavior).
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 3]],
  });
  await test_patterns({ nonPopup: "tab" });
  await SpecialPowers.popPrefEnv();

  // Non-popup is opened in a current tab.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 1]],
  });
  await test_patterns({ nonPopup: "current" });
  await SpecialPowers.popPrefEnv();

  // Non-popup is opened in a new window.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });
  await test_patterns({ nonPopup: "window" });
  await SpecialPowers.popPrefEnv();
});
