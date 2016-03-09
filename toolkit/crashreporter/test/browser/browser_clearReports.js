/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

function clickClearReports(browser) {
  let doc = content.document;

  let button = doc.getElementById("clear-reports");

  if (!button) {
    Assert.ok(false, "Button not found");
    return Promise.resolve();
  }

  let style = doc.defaultView.getComputedStyle(button, "");

  Assert.notEqual(style.display, "none", "Clear reports button visible");

  let deferred = {};
  deferred.promise = new Promise(resolve => deferred.resolve = resolve);
  var observer = new content.MutationObserver(function(mutations) {
    for (let mutation of mutations) {
      if (mutation.type == "attributes" &&
          mutation.attributeName == "style") {
        observer.disconnect();
        Assert.equal(style.display, "none", "Clear reports button hidden");
        deferred.resolve();
      }
    }
  });
  observer.observe(button, {
      attributes: true,
      childList: true,
      characterData: true,
      attributeFilter: ["style"],
  });

  button.click();
  return deferred.promise;
}

var promptShown = false;

var oldPrompt = Services.prompt;
Services.prompt = {
  confirm: function() {
    promptShown = true;
    return true;
  },
};

registerCleanupFunction(function () {
  Services.prompt = oldPrompt;
});

add_task(function* test() {
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");

  // Add crashes to submitted dir
  let submitdir = crD.clone();
  submitdir.append("submitted");

  let file1 = submitdir.clone();
  file1.append("bp-nontxt");
  file1.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  let file2 = submitdir.clone();
  file2.append("nonbp-file.txt");
  file2.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  add_fake_crashes(crD, 5);

  // Add crashes to pending dir
  let pendingdir = crD.clone();
  pendingdir.append("pending");

  let crashes = add_fake_crashes(crD, 2);
  addPendingCrashreport(crD, crashes[0].date);
  addPendingCrashreport(crD, crashes[1].date);

  // Add crashes to reports dir
  let report1 = crD.clone();
  report1.append("NotInstallTime777");
  report1.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  let report2 = crD.clone();
  report2.append("InstallTime" + Services.appinfo.appBuildID);
  report2.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  let report3 = crD.clone();
  report3.append("InstallTimeNew");
  report3.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  let report4 = crD.clone();
  report4.append("InstallTimeOld");
  report4.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  report4.lastModifiedTime = Date.now() - 63172000000;

  registerCleanupFunction(function () {
    cleanup_fake_appdir();
  });

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:crashes" },
    function* (browser) {
      let dirs = [ submitdir, pendingdir, crD ];
      let existing = [ file1.path, file2.path, report1.path, report2.path,
                       report3.path, submitdir.path, pendingdir.path ];

      yield ContentTask.spawn(browser, null, clickClearReports);

      for (let dir of dirs) {
        let entries = dir.directoryEntries;
        while (entries.hasMoreElements()) {
          let file = entries.getNext().QueryInterface(Ci.nsIFile);
          let index = existing.indexOf(file.path);
          isnot(index, -1, file.leafName + " exists");

          if (index != -1) {
            existing.splice(index, 1);
          }
        }
      }

      is(existing.length, 0, "All the files that should still exist exist");
      ok(promptShown, "Prompt shown");
    });
});
