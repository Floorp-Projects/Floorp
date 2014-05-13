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

const {
  PROCESS_TYPE_MAIN,
  PROCESS_TYPE_CONTENT,
  PROCESS_TYPE_PLUGIN,
  CRASH_TYPE_CRASH,
  CRASH_TYPE_HANG,
} = CrashManager.prototype;

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
  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", d));

  Assert.equal(s.crashesCount, 1);

  let crashes = s.crashes;
  Assert.equal(crashes.length, 1);
  let c = crashes[0];

  Assert.equal(c.id, "id1", "ID set properly.");
  Assert.equal(c.crashDate.getTime(), d.getTime(), "Date set.");

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);
});

add_task(function test_save_load() {
  let s = yield getStore();

  yield s.save();

  let d1 = new Date();
  let d2 = new Date(d1.getTime() - 10000);
  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", d1) &&
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", d2)
  );

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

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  // Duplicate.
  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_main_hang() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG);
  Assert.ok(c.isOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_content_crash() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_content_hang() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_HANG);
  Assert.ok(c.isOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_plugin_crash() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_plugin_hang() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_HANG);
  Assert.ok(c.isOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2);
});

add_task(function* test_add_mixed_types() {
  let s = yield getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mcrash", new Date()) &&
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mhang", new Date()) &&
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "ccrash", new Date()) &&
    s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "chang", new Date()) &&
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "pcrash", new Date()) &&
    s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "phang", new Date())
  );

  Assert.equal(s.crashesCount, 6);

  yield s.save();

  s._data.crashes.clear();
  Assert.equal(s.crashesCount, 0);

  yield s.load();

  Assert.equal(s.crashesCount, 6);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 1);
});

// Crashes added beyond the high water mark behave properly.
add_task(function* test_high_water() {
  let s = yield getStore();

  let d1 = new Date(2014, 0, 1, 0, 0, 0);
  let d2 = new Date(2014, 0, 2, 0, 0, 0);

  let i = 0;
  for (; i < s.HIGH_WATER_DAILY_THRESHOLD; i++) {
    Assert.ok(
      s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mc1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mc2" + i, d2) &&
      s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mh1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mh2" + i, d2) &&

      s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "cc1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "cc2" + i, d2) &&
      s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "ch1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "ch2" + i, d2) &&

      s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "pc1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "pc2" + i, d2) &&
      s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "ph1" + i, d1) &&
      s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "ph2" + i, d2)
    );
  }

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mc1" + i, d1) &&
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mc2" + i, d2) &&
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mh1" + i, d1) &&
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mh2" + i, d2)
  );

  Assert.ok(!s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "cc1" + i, d1));
  Assert.ok(!s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH, "cc2" + i, d2));
  Assert.ok(!s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "ch1" + i, d1));
  Assert.ok(!s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "ch2" + i, d2));

  Assert.ok(!s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "pc1" + i, d1));
  Assert.ok(!s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH, "pc2" + i, d2));
  Assert.ok(!s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "ph1" + i, d1));
  Assert.ok(!s.addCrash(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG, "ph2" + i, d2));

  // We preserve main process crashes and hangs. Content and plugin crashes and
  // hangs beyond should be discarded.
  Assert.equal(s.crashesCount, 12 * s.HIGH_WATER_DAILY_THRESHOLD + 4);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD + 2);
  crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD + 2);

  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);

  crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);
  crashes = s.getCrashesOfType(PROCESS_TYPE_PLUGIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);

  // But raw counts should be preserved.
  let day1 = bsp.dateToDays(d1);
  let day2 = bsp.dateToDays(d2);
  Assert.ok(s._countsByDay.has(day1));
  Assert.ok(s._countsByDay.has(day2));

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);

  yield s.save();
  yield s.load();

  Assert.ok(s._countsByDay.has(day1));
  Assert.ok(s._countsByDay.has(day2));

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);

  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_CRASH),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
  Assert.equal(s._countsByDay.get(day1).
                 get(PROCESS_TYPE_PLUGIN + "-" + CRASH_TYPE_HANG),
               s.HIGH_WATER_DAILY_THRESHOLD + 1);
});
