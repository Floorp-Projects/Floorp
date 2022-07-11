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
        '<html><head></head><body><script src="script.js"></script><iframe src="https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/container2.html"></iframe></body></html>',
      "script.js":
        'window.count=0;window.p=new Promise(resolve=>{onmessage=e=>{count=e.data.data;resolve();};});p.then(()=>{document.documentElement.setAttribute("count",count);});',
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
  await ContentTask.spawn(browser, [], async function(obj) {
    // Need to wait a bit for cross-process postMessage...
    await ContentTaskUtils.waitForCondition(
      () => content.document.documentElement.getAttribute("count") !== null,
      "waiting for 'count' attribute"
    );
    let count = content.document.documentElement.getAttribute("count");
    is(count, 3, "Expected script nodes found");
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
  await extension.unload();
});
