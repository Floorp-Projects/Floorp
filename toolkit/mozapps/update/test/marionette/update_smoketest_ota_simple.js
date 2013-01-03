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
  let gotDownloading = false;
  let progress = 0, total = 0;

  addChromeEventListener("update-downloading", function(evt) {
    gotDownloading = true;
    return true;
  });
  addChromeEventListener("update-progress", function(evt) {
    progress = evt.detail.progress;
    total = evt.detail.total;
    if (total == progress) {
      ok(gotDownloading);
      return true;
    }
    return false;
  });
  addChromeEventListener("update-downloaded", function(evt) {
    ok(gotDownloading);
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
