/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");

const EXAMPLE_2014052701 = {
  "upgradedFrom":"13.0.1",
  "uninstallReason":"SUCCESSFUL_UPGRADE",
  "_entityID":null,
  "forensicsID":"29525548-b653-49db-bfb8-a160cdfbeb4a",
  "_installInProgress":false,
  "_everCompatible":true,
  "reportedWindowsVersion":[6,1,1],
  "actualWindowsVersion":[6,1,1],
  "firstNotifyDay":0,
  "lastNotifyDay":0,
  "downloadAttempts":1,
  "downloadFailures":0,
  "installAttempts":1,
  "installSuccesses":1,
  "installLauncherFailures":0,
  "installFailures":0,
  "notificationsShown":0,
  "notificationsClicked":0,
  "notificationsDismissed":0,
  "notificationsRemoved":0,
  "launcherExitCodes":{"0":1}
};

function run_test() {
  run_next_test();
}

add_task(function* init() {
  do_get_profile();
});

add_task(function test_constructor() {
  new HotfixProvider();
});

add_task(function* test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new HotfixProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function* test_collect_empty() {
  let storage = yield Metrics.Storage("collect_empty");
  let provider = new HotfixProvider();
  yield provider.init(storage);

  yield provider.collectDailyData();

  let m = provider.getMeasurement("update", 1);
  let data = yield m.getValues();
  Assert.equal(data.singular.size, 0);
  Assert.equal(data.days.size, 0);

  yield storage.close();
});

add_task(function* test_collect_20140527() {
  let storage = yield Metrics.Storage("collect_20140527");
  let provider = new HotfixProvider();
  yield provider.init(storage);

  let path = OS.Path.join(OS.Constants.Path.profileDir,
                          "hotfix.v20140527.01.json");
  let encoder = new TextEncoder();
  yield OS.File.writeAtomic(path,
                            encoder.encode(JSON.stringify(EXAMPLE_2014052701)));

  yield provider.collectDailyData();

  let m = provider.getMeasurement("update", 1);
  let data = yield m.getValues();
  let s = data.singular;
  Assert.equal(s.size, 7);
  Assert.equal(s.get("v20140527.upgradedFrom")[1], "13.0.1");
  Assert.equal(s.get("v20140527.uninstallReason")[1], "SUCCESSFUL_UPGRADE");
  Assert.equal(s.get("v20140527.downloadAttempts")[1], 1);
  Assert.equal(s.get("v20140527.downloadFailures")[1], 0);
  Assert.equal(s.get("v20140527.installAttempts")[1], 1);
  Assert.equal(s.get("v20140527.installFailures")[1], 0);
  Assert.equal(s.get("v20140527.notificationsShown")[1], 0);

  // Ensure the dynamic fields get serialized.
  let serializer = m.serializer(m.SERIALIZE_JSON);
  let d = serializer.singular(s);

  Assert.deepEqual(d, {
    "_v": 1,
    "v20140527.upgradedFrom": "13.0.1",
    "v20140527.uninstallReason": "SUCCESSFUL_UPGRADE",
    "v20140527.downloadAttempts": 1,
    "v20140527.downloadFailures": 0,
    "v20140527.installAttempts": 1,
    "v20140527.installFailures": 0,
    "v20140527.notificationsShown": 0,
  });

  // Don't interfere with next test.
  yield OS.File.remove(path);

  yield storage.close();
});

add_task(function* test_collect_multiple_versions() {
  let storage = yield Metrics.Storage("collect_multiple_versions");
  let provider = new HotfixProvider();
  yield provider.init(storage);

  let p1 = {
    upgradedFrom: "12.0",
    uninstallReason: "SUCCESSFUL_UPGRADE",
    downloadAttempts: 3,
    downloadFailures: 1,
    installAttempts: 1,
    installFailures: 1,
    notificationsShown: 2,
  };

  let p2 = {
    downloadAttempts: 5,
    downloadFailures: 3,
    installAttempts: 2,
    installFailures: 2,
    uninstallReason: null,
    notificationsShown: 1,
  };

  let path1 = OS.Path.join(OS.Constants.Path.profileDir, "updateHotfix.v20140601.json");
  let path2 = OS.Path.join(OS.Constants.Path.profileDir, "updateHotfix.v20140701.json");

  let encoder = new TextEncoder();
  yield OS.File.writeAtomic(path1, encoder.encode(JSON.stringify(p1)));
  yield OS.File.writeAtomic(path2, encoder.encode(JSON.stringify(p2)));

  yield provider.collectDailyData();

  let m = provider.getMeasurement("update", 1);
  let data = yield m.getValues();

  let serializer = m.serializer(m.SERIALIZE_JSON);
  let d = serializer.singular(data.singular);

  Assert.deepEqual(d, {
    "_v": 1,
    "v20140601.upgradedFrom": "12.0",
    "v20140601.uninstallReason": "SUCCESSFUL_UPGRADE",
    "v20140601.downloadAttempts": 3,
    "v20140601.downloadFailures": 1,
    "v20140601.installAttempts": 1,
    "v20140601.installFailures": 1,
    "v20140601.notificationsShown": 2,
    "v20140701.uninstallReason": "STILL_INSTALLED",
    "v20140701.downloadAttempts": 5,
    "v20140701.downloadFailures": 3,
    "v20140701.installAttempts": 2,
    "v20140701.installFailures": 2,
    "v20140701.notificationsShown": 1,
  });

  // Don't interfere with next test.
  yield OS.File.remove(path1);
  yield OS.File.remove(path2);

  yield storage.close();
});
