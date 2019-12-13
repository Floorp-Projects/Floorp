add_task(async function() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences" },
    async function(browser) {
      let newTabURL = "http://www.example.com/";
      await SpecialPowers.spawn(browser, [newTabURL], async function(
        newTabURL
      ) {
        let doc = content.document;
        let label = doc.createXULElement("label", { is: "text-link" });
        label.href = newTabURL;
        label.id = "textlink-test";
        label.textContent = "click me";
        doc.body.append(label);
      });

      // Test that click will open tab in foreground.
      let awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#textlink-test",
        {},
        browser
      );
      let newTab = await awaitNewTab;
      is(
        newTab.linkedBrowser,
        gBrowser.selectedBrowser,
        "selected tab should be example page"
      );
      BrowserTestUtils.removeTab(gBrowser.selectedTab);

      // Test that ctrl+shift+click/meta+shift+click will open tab in background.
      awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#textlink-test",
        { ctrlKey: true, metaKey: true, shiftKey: true },
        browser
      );
      await awaitNewTab;
      is(
        gBrowser.selectedBrowser,
        browser,
        "selected tab should be original tab"
      );
      BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);

      // Middle-clicking should open tab in foreground.
      awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#textlink-test",
        { button: 1 },
        browser
      );
      newTab = await awaitNewTab;
      is(
        newTab.linkedBrowser,
        gBrowser.selectedBrowser,
        "selected tab should be example page"
      );
      BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
    }
  );
});
