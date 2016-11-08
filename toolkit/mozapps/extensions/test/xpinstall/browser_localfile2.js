// ----------------------------------------------------------------------------
// Test whether an install fails if the url is a local file when requested from
// web content
add_task(function* test() {
  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);

  var chromeroot = getChromeRoot(gTestPath);
  var xpipath = chromeroot + "amosigned.xpi";
  try {
    xpipath = cr.convertChromeURL(makeURI(xpipath)).spec;
  } catch (ex) {
    // scenario where we are running from a .jar and already extracted
  }

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": xpipath
  }));

  // In non-e10s the exception in the content page would trigger a test failure
  if (!gMultiProcessBrowser)
    expectUncaughtException();

  let URI = TESTROOT + "installtrigger.html?" + triggers;
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    yield ContentTask.spawn(browser, URI, function* (URI) {
      content.location.href = URI;

      let loaded = ContentTaskUtils.waitForEvent(this, "load", true);
      let installTriggered = ContentTaskUtils.waitForEvent(this, "InstallTriggered", true, null, true);
      yield Promise.all([ loaded, installTriggered ]);

      let doc = content.document;
      is(doc.getElementById("return").textContent, "exception", "installTrigger should have failed");
    });
  });
});
// ----------------------------------------------------------------------------
