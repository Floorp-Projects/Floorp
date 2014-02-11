/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file tests the CrashStore type in CrashManager.jsm.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

let bsp = Cu.import("resource://gre/modules/CrashManager.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

const CrashStore = bsp.CrashStore;

let STORE_DIR_COUNT = 0;

function getStore() {
  return Task.spawn(function* () {
    let storeDir = do_get_tempdir().path;
    storeDir = OS.Path.join(storeDir, "store-" + STORE_DIR_COUNT++);

    yield OS.File.makeDir(storeDir, {unixMode: OS.Constants.libc.S_IRWXU});

    let s = new CrashStore(storeDir);
    yield s.load();

    return s;
  });
}

function run_test() {
  run_next_test();
}

add_task(function* test_constructor() {
  let s = new CrashStore("/some/path");
  Assert.ok(s instanceof CrashStore);
});

add_task(function test_add_crash() {
  let s = yield getStore();

  Assert.equal(s.crashesCount, 0);
  let d = new Date(Date.now() - 5000);
  s.addMainProcessCrash("id1", d);

  Assert.equal(s.crashesCount, 1);

  let crashes = s.crashes;
  Assert.equal(crashes.length, 1);
  let c = crashes[0];

  Assert.equal(c.id, "id1", "ID set properly.");
  Assert.equal(c.crashDate.getTime(), d.getTime(), "Date set.");

  s.addMainProcessCrash("id2", new Date());
  Assert.equal(s.crashesCount, 2);
});

add_task(function test_save_load() {
  let s = yield getStore();

  yield s.save();

  let d1 = new Date();
  let d2 = new Date(d1.getTime() - 10000);
  s.addMainProcessCrash("id1", d1);
  s.addMainProcessCrash("id2", d2);

  yield s.save();

  yield s.load();
  Assert.ok(!s.corruptDate);
  let crashes = s.crashes;

  Assert.equal(crashes.length, 2);
  let c = s.getCrash("id1");
  Assert.equal(c.crashDate.getTime(), d1.getTime());
});

add_task(function test_corrupt_json() {
  let s = yield getStore();

  let buffer = new TextEncoder().encode("{bad: json-file");
  yield OS.File.writeAtomic(s._storePath, buffer, {compression: "lz4"});

  yield s.load();
  Assert.ok(s.corruptDate, "Corrupt date is defined.");

  let date = s.corruptDate;
  yield s.save();
  s._data = null;
  yield s.load();
  Assert.ok(s.corruptDate);
  Assert.equal(date.getTime(), s.corruptDate.getTime());
});

add_task(function* test_add_main_crash() {
  let s = yield getStore();

  s.addMainProcessCrash("id1", new Date());
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, bsp.CrashStore.prototype.TYPE_MAIN_CRASH);
  Assert.ok(c.isMainProcessCrash);

  s.addMainProcessCrash("id2", new Date());
  Assert.equal(s.crashesCount, 2);

  // Duplicate.
  s.addMainProcessCrash("id1", new Date());
  Assert.equal(s.crashesCount, 2);

  Assert.equal(s.mainProcessCrashes.length, 2);
});

add_task(function* test_add_plugin_crash() {
  let s = yield getStore();

  s.addPluginCrash("id1", new Date());
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, bsp.CrashStore.prototype.TYPE_PLUGIN_CRASH);
  Assert.ok(c.isPluginCrash);

  s.addPluginCrash("id2", new Date());
  Assert.equal(s.crashesCount, 2);

  s.addPluginCrash("id1", new Date());
  Assert.equal(s.crashesCount, 2);

  Assert.equal(s.pluginCrashes.length, 2);
});

add_task(function* test_add_plugin_hang() {
  let s = yield getStore();

  s.addPluginHang("id1", new Date());
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, bsp.CrashStore.prototype.TYPE_PLUGIN_HANG);
  Assert.ok(c.isPluginHang);

  s.addPluginHang("id2", new Date());
  Assert.equal(s.crashesCount, 2);

  s.addPluginHang("id1", new Date());
  Assert.equal(s.crashesCount, 2);

  Assert.equal(s.pluginHangs.length, 2);
});

add_task(function* test_add_mixed_types() {
  let s = yield getStore();

  s.addMainProcessCrash("main", new Date());
  s.addPluginCrash("pcrash", new Date());
  s.addPluginHang("phang", new Date());

  Assert.equal(s.crashesCount, 3);

  yield s.save();

  s._data.crashes.clear();
  Assert.equal(s.crashesCount, 0);

  yield s.load();

  Assert.equal(s.crashesCount, 3);

  Assert.equal(s.mainProcessCrashes.length, 1);
  Assert.equal(s.pluginCrashes.length, 1);
  Assert.equal(s.pluginHangs.length, 1);
});
