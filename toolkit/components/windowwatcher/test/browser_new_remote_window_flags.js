/**
 * Tests that when a remote browser opens a new window that the
 * newly opened window is also remote.
 */

const ANCHOR_PAGE = `data:text/html,<a href="about:blank" target="_blank">Click me!</a>`;
const SCRIPT_PAGE = `data:text/html,<script>window.open("about:blank", "_blank");</script>`;

// This magic value of 2 means that by default, when content tries
// to open a new window, it'll actually open in a new window instead
// of a new tab.
add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });
});

function assertFlags(win) {
  let docShell = win.docShell;
  let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
  let chromeFlags = docShell.treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIAppWindow).chromeFlags;
  Assert.ok(
    loadContext.useRemoteTabs,
    "Should be using remote tabs on the load context"
  );
  Assert.ok(
    chromeFlags & Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW,
    "Should have the remoteness chrome flag on the window"
  );
}

/**
 * Content can open a window using a target="_blank" link
 */
add_task(async function test_new_remote_window_flags_target_blank() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: ANCHOR_PAGE,
    },
    async function(browser) {
      let newWinPromise = BrowserTestUtils.waitForNewWindow();
      await BrowserTestUtils.synthesizeMouseAtCenter("a", {}, browser);
      let win = await newWinPromise;
      assertFlags(win);
      await BrowserTestUtils.closeWindow(win);
    }
  );
});

/**
 * Content can open a window using window.open
 */
add_task(async function test_new_remote_window_flags_window_open() {
  let newWinPromise = BrowserTestUtils.waitForNewWindow();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SCRIPT_PAGE,
    },
    async function(browser) {
      let win = await newWinPromise;
      assertFlags(win);
      await BrowserTestUtils.closeWindow(win);
    }
  );
});

/**
 * Privileged content scripts can also open new windows
 * using content.open.
 */
add_task(async function test_new_remote_window_flags_content_open() {
  let newWinPromise = BrowserTestUtils.waitForNewWindow();
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.open("about:blank", "_blank");
  });

  let win = await newWinPromise;
  assertFlags(win);
  await BrowserTestUtils.closeWindow(win);
});
