// ----------------------------------------------------------------------------
// Test whether an install fails if the url is a local file when requested from
// web content
add_task(async function test() {
  var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  var chromeroot = getChromeRoot(gTestPath);
  var xpipath = chromeroot + "amosigned.xpi";
  try {
    xpipath = cr.convertChromeURL(makeURI(xpipath)).spec;
  } catch (ex) {
    // scenario where we are running from a .jar and already extracted
  }

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": xpipath,
    })
  );

  // In non-e10s the exception in the content page would trigger a test failure
  if (!gMultiProcessBrowser) {
    expectUncaughtException();
  }

  let URI = TESTROOT + "installtrigger.html?" + triggers;
  await BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT }, async function(
    browser
  ) {
    await SpecialPowers.spawn(browser, [URI], async function(URI) {
      content.location.href = URI;

      let loaded = ContentTaskUtils.waitForEvent(
        docShell.chromeEventHandler,
        "load",
        true
      );
      let installTriggered = ContentTaskUtils.waitForEvent(
        docShell.chromeEventHandler,
        "InstallTriggered",
        true,
        null,
        true
      );
      await Promise.all([loaded, installTriggered]);

      let doc = content.document;
      is(
        doc.getElementById("return").textContent,
        "exception",
        "installTrigger should have failed"
      );
    });
  });
});
// ----------------------------------------------------------------------------
