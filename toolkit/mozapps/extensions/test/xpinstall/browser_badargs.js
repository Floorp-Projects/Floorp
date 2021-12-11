// ----------------------------------------------------------------------------
// Test whether passing a simple string to InstallTrigger.install throws an
// exception
function test() {
  waitForExplicitFinish();

  var triggers = encodeURIComponent(JSON.stringify(TESTROOT + "amosigned.xpi"));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TESTROOT);

  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return new Promise(resolve => {
      addEventListener(
        "load",
        () => {
          content.addEventListener("InstallTriggered", () => {
            resolve(content.document.getElementById("return").textContent);
          });
        },
        true
      );
    });
  }).then(page_loaded);

  // In non-e10s the exception in the content page would trigger a test failure
  if (!gMultiProcessBrowser) {
    expectUncaughtException();
  }

  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function page_loaded(result) {
  is(result, "exception", "installTrigger should have failed");

  // In non-e10s the exception from the page is thrown after the event so we
  // have to spin the event loop to make sure it arrives so expectUncaughtException
  // sees it.
  executeSoon(() => {
    gBrowser.removeCurrentTab();
    finish();
  });
}
// ----------------------------------------------------------------------------
