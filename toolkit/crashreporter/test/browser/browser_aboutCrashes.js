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

        const submitted = doc.getElementById("reportListSubmitted");
        Assert.ok(
          !submitted.classList.contains("hidden"),
          "the submitted crash list is visible"
        );
        const unsubmitted = doc.getElementById("reportListUnsubmitted");
        Assert.ok(
          unsubmitted.classList.contains("hidden"),
          "the unsubmitted crash list is hidden"
        );

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

  clear_fake_crashes(crD, crashes);
  const pendingCrash = addPendingCrashreport(crD, Date.now(), { foo: "bar" });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:crashes" },
    browser => {
      info("about:crashes loaded");
      return ContentTask.spawn(browser, pendingCrash, pendingCrash => {
        const doc = content.document;

        const submitted = doc.getElementById("reportListSubmitted");
        Assert.ok(
          submitted.classList.contains("hidden"),
          "the submitted crash list is hidden"
        );
        const unsubmitted = doc.getElementById("reportListUnsubmitted");
        Assert.ok(
          !unsubmitted.classList.contains("hidden"),
          "the unsubmitted crash list is visible"
        );

        const crashIds = doc.getElementsByClassName("crash-id");
        Assert.equal(
          crashIds.length,
          1,
          "about:crashes lists correct number of crash reports"
        );
        const pendingRow = doc.getElementById(pendingCrash.id);
        Assert.equal(
          pendingRow.cells[0].textContent,
          pendingCrash.id,
          "about:crashes lists pending crash IDs correctly"
        );
      });
    }
  );

  cleanup_fake_appdir();
});
