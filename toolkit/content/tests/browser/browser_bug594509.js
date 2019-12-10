add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:rights"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    Assert.ok(
      content.document.getElementById("your-rights"),
      "about:rights content loaded"
    );
  });

  BrowserTestUtils.removeTab(tab);
});
