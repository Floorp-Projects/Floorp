/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function testForceCheck() {
  addChromeEventListener("update-available", function(evt) {
    let update = evt.detail;
    is(update.displayVersion, "99.0");
    is(update.isOSUpdate, false);
    statusSettingIs("check-complete", testDownload);
    return true;
  });
  sendContentEvent("force-update-check");
}

function testDownload() {
  let gotStarted = false, gotProgress = false, gotStopped = false;
  let progress = 0, total = 0;

  addChromeEventListener("update-download-started", function(evt) {
    gotStarted = true;
    return true;
  });
  addChromeEventListener("update-download-progress", function(evt) {
    progress = evt.detail.progress;
    total = evt.detail.total;
    gotProgress = true;
    if (total == progress) {
      ok(gotStarted);
      return true;
    }
    return false;
  });
  addChromeEventListener("update-download-stopped", function(evt) {
    is(evt.detail.paused, false);
    gotStopped = true;
    ok(gotStarted);
    ok(gotProgress);
    return true;
  });
  addChromeEventListener("update-downloaded", function(evt) {
    ok(gotStarted);
    ok(gotProgress);
    ok(gotStopped);
    is(progress, total);
    return true;
  });
  addChromeEventListener("update-prompt-apply", function(evt) {
    let update = evt.detail;
    is(update.displayVersion, "99.0");
    is(update.isOSUpdate, false);
    cleanUp();
  });
  sendContentEvent("update-available-result", {
    result: "download"
  });
}

function testApplied() {
  let updateFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  updateFile.initWithPath("/system/b2g/update_test/UpdateTestAddFile");
  ok(updateFile.exists());
  cleanUp();
}

// Update lifecycle callbacks
function preUpdate() {
  testForceCheck();
}

function postUpdate() {
  testApplied();
}
