add_task(async function test() {
  const appD = make_fake_appdir();
  const crD = appD.clone();
  crD.append("Crash Reports");
  const crashes = add_fake_crashes(crD, 5);
  // sanity check
  const appDtest = Services.dirsvc.get("UAppData", Ci.nsIFile);
  ok(appD.equals(appDtest), "directory service provider registered ok");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:crashes" },
    browser => {
      info("about:crashes loaded");
      return ContentTask.spawn(browser, crashes, crashes => {
        const doc = content.document;
        const crashIds = doc.getElementsByClassName("crash-id");
        Assert.equal(
          crashIds.length,
          crashes.length,
          "about:crashes lists correct number of crash reports"
        );
        for (let i = 0; i < crashes.length; i++) {
          Assert.equal(
            crashIds[i].textContent,
            crashes[i].id,
            i + ": crash ID is correct"
          );
        }
      });
    }
  );

  cleanup_fake_appdir();
});
