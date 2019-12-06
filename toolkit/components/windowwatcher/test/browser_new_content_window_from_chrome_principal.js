"use strict";

/**
 * Tests that if chrome-privileged code calls .open() on an
 * unprivileged window, that the principal in the newly
 * opened window is appropriately set.
 */
add_task(async function test_chrome_opens_window() {
  // This magic value of 2 means that by default, when content tries
  // to open a new window, it'll actually open in a new window instead
  // of a new tab.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });

  let newWinPromise = BrowserTestUtils.waitForNewWindow({
    url: "http://example.com/",
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.open("http://example.com/", "_blank");
  });

  let win = await newWinPromise;
  let browser = win.gBrowser.selectedBrowser;
  Assert.ok(
    E10SUtils.isWebRemoteType(browser.remoteType),
    "Should have the default content remote type."
  );

  await SpecialPowers.spawn(browser, [], async function() {
    Assert.ok(
      !content.document.nodePrincipal.isSystemPrincipal,
      "We should not have a system principal."
    );
    Assert.equal(
      content.document.nodePrincipal.origin,
      "http://example.com",
      "Should have the example.com principal"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});
