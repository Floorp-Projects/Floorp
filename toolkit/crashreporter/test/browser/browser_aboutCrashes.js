add_task(function* test() {
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");
  let crashes = add_fake_crashes(crD, 5);
  // sanity check
  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let appDtest = dirSvc.get("UAppData", Components.interfaces.nsILocalFile);
  ok(appD.equals(appDtest), "directory service provider registered ok");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:crashes" }, function(browser) {
    info("about:crashes loaded");
    return ContentTask.spawn(browser, crashes, function(crashes) {
      let doc = content.document;
      let crashlinks = doc.getElementById("submitted").querySelectorAll(".crashReport");
      Assert.equal(crashlinks.length, crashes.length,
        "about:crashes lists correct number of crash reports");
      for (let i = 0; i < crashes.length; i++) {
        Assert.equal(crashlinks[i].firstChild.textContent, crashes[i].id,
          i + ": crash ID is correct");
      }
    });
  });

  cleanup_fake_appdir();
});
