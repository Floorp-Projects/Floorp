browser.test.log("running background script");

browser.test.onMessage.addListener((x, y) => {
  browser.test.assertEq(x, 10, "x is 10");
  browser.test.assertEq(y, 20, "y is 20");

  browser.test.notifyPass("background test passed");
});

browser.test.sendMessage("running", 1);
