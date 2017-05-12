add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:rights");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    Assert.ok(content.document.getElementById("your-rights"), "about:rights content loaded");
  });

  await BrowserTestUtils.removeTab(tab);
});
