function check_about_rights(tab) {
  let doc = gBrowser.getBrowserForTab(tab).contentDocument;
  ok(doc.getElementById("your-rights"), "about:rights content loaded");
  gBrowser.removeTab(tab);
  finish();
}

function test() {
  waitForExplicitFinish();
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      ok(true, "about:rights loaded");
      executeSoon(function() { check_about_rights(tab); });
    }, true);
  browser.loadURI("about:rights", null, null);
}
