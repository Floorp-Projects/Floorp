ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  info("Starting test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.trackingprotection.enabled", true]],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["https://tracking.example.com/"] },
    files: {
      "page.html":
        '<html><head></head><body><iframe src="https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/container.html"></iframe></body></html>',
    },
    async background() {
      browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
    },
  });
  await extension.startup();
  let url = await extension.awaitMessage("ready");

  info("Creating a new tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;

  info("Verify the number of script nodes found");
  await ContentTask.spawn(browser, {}, async function(obj) {
    let doc = content.document.querySelector("iframe").contentDocument;
    doc = doc.querySelector("iframe").contentDocument;
    let scripts = doc.querySelectorAll("script");
    is(scripts.length, 3, "Expected script nodes found");
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
  await extension.unload();
});
