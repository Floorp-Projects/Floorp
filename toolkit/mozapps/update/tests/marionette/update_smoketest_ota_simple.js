/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function testForceCheck() {
  addChromeEventListener("update-available", function(evt) {
    isFinishUpdate(evt.detail);
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
    isStartToFinishUpdate(evt.detail);
    cleanUp();
  });
  sendContentEvent("update-available-result", {
    result: "download"
  });
}

function testApplied() {
  let finish = getFinishBuild();
  is(Services.appinfo.version, finish.app_version,
     "Services.appinfo.version should be " + finish.app_version);
  is(Services.appinfo.platformVersion, finish.platform_milestone,
     "Services.appinfo.platformVersion should be " + finish.platform_milestone);
  is(Services.appinfo.appBuildID, finish.app_build_id,
     "Services.appinfo.appBuildID should be " + finish.app_build_id);
  cleanUp();
}

// Update lifecycle callbacks
function preUpdate() {
  testForceCheck();
}

function postUpdate() {
  testApplied();
}
