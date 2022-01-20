/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file tests the CrashStore type in CrashManager.jsm.
 */

"use strict";

var { CrashManager, CrashStore, dateToDays } = ChromeUtils.import(
  "resource://gre/modules/CrashManager.jsm",
  null
);

const DUMMY_DATE = new Date(Date.now() - 10 * 24 * 60 * 60 * 1000);
DUMMY_DATE.setMilliseconds(0);

const DUMMY_DATE_2 = new Date(Date.now() - 5 * 24 * 60 * 60 * 1000);
DUMMY_DATE_2.setMilliseconds(0);

const {
  PROCESS_TYPE_MAIN,
  PROCESS_TYPE_CONTENT,
  PROCESS_TYPE_GMPLUGIN,
  PROCESS_TYPE_GPU,
  PROCESS_TYPE_VR,
  PROCESS_TYPE_RDD,
  PROCESS_TYPE_SOCKET,
  CRASH_TYPE_CRASH,
  CRASH_TYPE_HANG,
  SUBMISSION_RESULT_OK,
  SUBMISSION_RESULT_FAILED,
} = CrashManager.prototype;

var STORE_DIR_COUNT = 0;

function getStore() {
  return (async function() {
    let storeDir = do_get_tempdir().path;
    storeDir = PathUtils.join(storeDir, "store-" + STORE_DIR_COUNT++);

    await IOUtils.makeDirectory(storeDir, { permissions: 0o700 });

    let s = new CrashStore(storeDir);
    await s.load();

    return s;
  })();
}

add_task(async function test_constructor() {
  let s = new CrashStore(do_get_tempdir().path);
  Assert.ok(s instanceof CrashStore);
});

add_task(async function test_add_crash() {
  let s = await getStore();

  Assert.equal(s.crashesCount, 0);
  let d = new Date(Date.now() - 5000);
  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", d));

  Assert.equal(s.crashesCount, 1);

  let crashes = s.crashes;
  Assert.equal(crashes.length, 1);
  let c = crashes[0];

  Assert.equal(c.id, "id1", "ID set properly.");
  Assert.equal(c.crashDate.getTime(), d.getTime(), "Date set.");

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);
});

add_task(async function test_reset() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", DUMMY_DATE));
  Assert.equal(s.crashes.length, 1);
  s.reset();
  Assert.equal(s.crashes.length, 0);
});

add_task(async function test_save_load() {
  let s = await getStore();

  await s.save();

  let d1 = new Date();
  let d2 = new Date(d1.getTime() - 10000);
  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", d1));
  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", d2));
  Assert.ok(s.addSubmissionAttempt("id1", "sub1", d1));
  Assert.ok(s.addSubmissionResult("id1", "sub1", d2, SUBMISSION_RESULT_OK));
  Assert.ok(s.setRemoteCrashID("id1", "bp-1"));

  await s.save();

  await s.load();
  Assert.ok(!s.corruptDate);
  let crashes = s.crashes;

  Assert.equal(crashes.length, 2);
  let c = s.getCrash("id1");
  Assert.equal(c.crashDate.getTime(), d1.getTime());
  Assert.equal(c.remoteID, "bp-1");

  Assert.ok(!!c.submissions);
  let submission = c.submissions.get("sub1");
  Assert.ok(!!submission);
  Assert.equal(submission.requestDate.getTime(), d1.getTime());
  Assert.equal(submission.responseDate.getTime(), d2.getTime());
  Assert.equal(submission.result, SUBMISSION_RESULT_OK);
});

add_task(async function test_corrupt_json() {
  let s = await getStore();

  let buffer = new TextEncoder().encode("{bad: json-file");
  await IOUtils.write(s._storePath, buffer, { compress: true });

  await s.load();
  Assert.ok(s.corruptDate, "Corrupt date is defined.");

  let date = s.corruptDate;
  await s.save();
  s._data = null;
  await s.load();
  Assert.ok(s.corruptDate);
  Assert.equal(date.getTime(), s.corruptDate.getTime());
});

add_task(async function test_add_main_crash() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH));

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);

  // Duplicate.
  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "id3", new Date(), {
      OOMAllocationSize: 1048576,
    })
  );
  Assert.equal(s.crashesCount, 3);
  Assert.deepEqual(s.crashes[2].metadata, { OOMAllocationSize: 1048576 });

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 3);
});

add_task(async function test_add_main_hang() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id1", new Date()));
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG);
  Assert.ok(c.isOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG));

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);

  Assert.ok(s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "id1", new Date()));
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_content_crash() {
  let s = await getStore();

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

add_task(async function test_add_content_hang() {
  let s = await getStore();

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

add_task(async function test_add_gmplugin_crash() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_GMPLUGIN + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_gpu_crash() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_GPU + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH));

  Assert.ok(s.addCrash(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);

  Assert.ok(s.addCrash(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_vr_crash() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_VR, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_VR + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_VR, CRASH_TYPE_CRASH));

  Assert.ok(s.addCrash(PROCESS_TYPE_VR, CRASH_TYPE_CRASH, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);

  Assert.ok(s.addCrash(PROCESS_TYPE_VR, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_VR, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_rdd_crash() {
  let s = await getStore();

  Assert.ok(s.addCrash(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_RDD + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH));

  Assert.ok(s.addCrash(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH, "id2", new Date()));
  Assert.equal(s.crashesCount, 2);

  Assert.ok(s.addCrash(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH, "id1", new Date()));
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_socket_crash() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 1);

  let c = s.crashes[0];
  Assert.ok(c.crashDate);
  Assert.equal(c.type, PROCESS_TYPE_SOCKET + "-" + CRASH_TYPE_CRASH);
  Assert.ok(c.isOfType(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH));

  Assert.ok(
    s.addCrash(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH, "id2", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  Assert.ok(
    s.addCrash(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH, "id1", new Date())
  );
  Assert.equal(s.crashesCount, 2);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2);
});

add_task(async function test_add_mixed_types() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "mcrash", new Date()) &&
      s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG, "mhang", new Date()) &&
      s.addCrash(
        PROCESS_TYPE_CONTENT,
        CRASH_TYPE_CRASH,
        "ccrash",
        new Date()
      ) &&
      s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "chang", new Date()) &&
      s.addCrash(
        PROCESS_TYPE_GMPLUGIN,
        CRASH_TYPE_CRASH,
        "gmpcrash",
        new Date()
      ) &&
      s.addCrash(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH, "gpucrash", new Date()) &&
      s.addCrash(PROCESS_TYPE_VR, CRASH_TYPE_CRASH, "vrcrash", new Date()) &&
      s.addCrash(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH, "rddcrash", new Date()) &&
      s.addCrash(
        PROCESS_TYPE_SOCKET,
        CRASH_TYPE_CRASH,
        "socketcrash",
        new Date()
      )
  );

  Assert.equal(s.crashesCount, 9);

  await s.save();

  s._data.crashes.clear();
  Assert.equal(s.crashesCount, 0);

  await s.load();

  Assert.equal(s.crashesCount, 9);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_GMPLUGIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_GPU, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_VR, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_RDD, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
  crashes = s.getCrashesOfType(PROCESS_TYPE_SOCKET, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 1);
});

// Crashes added beyond the high water mark behave properly.
add_task(async function test_high_water() {
  let s = await getStore();

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
        s.addCrash(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG, "ch2" + i, d2)
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

  // We preserve main process crashes and hangs. Content crashes and
  // hangs beyond should be discarded.
  Assert.equal(s.crashesCount, 8 * s.HIGH_WATER_DAILY_THRESHOLD + 4);

  let crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD + 2);
  crashes = s.getCrashesOfType(PROCESS_TYPE_MAIN, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD + 2);

  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_CRASH);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);
  crashes = s.getCrashesOfType(PROCESS_TYPE_CONTENT, CRASH_TYPE_HANG);
  Assert.equal(crashes.length, 2 * s.HIGH_WATER_DAILY_THRESHOLD);

  // But raw counts should be preserved.
  let day1 = dateToDays(d1);
  let day2 = dateToDays(d2);
  Assert.ok(s._countsByDay.has(day1));
  Assert.ok(s._countsByDay.has(day2));

  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );
  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );

  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_CRASH),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );
  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_HANG),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );

  await s.save();
  await s.load();

  Assert.ok(s._countsByDay.has(day1));
  Assert.ok(s._countsByDay.has(day2));

  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_CRASH),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );
  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_MAIN + "-" + CRASH_TYPE_HANG),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );

  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_CRASH),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );
  Assert.equal(
    s._countsByDay.get(day1).get(PROCESS_TYPE_CONTENT + "-" + CRASH_TYPE_HANG),
    s.HIGH_WATER_DAILY_THRESHOLD + 1
  );
});

add_task(async function test_addSubmission() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "crash1", DUMMY_DATE)
  );

  Assert.ok(s.addSubmissionAttempt("crash1", "sub1", DUMMY_DATE));

  let crash = s.getCrash("crash1");
  let submission = crash.submissions.get("sub1");
  Assert.ok(!!submission);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE.getTime());
  Assert.equal(submission.responseDate, null);
  Assert.equal(submission.result, null);

  Assert.ok(
    s.addSubmissionResult(
      "crash1",
      "sub1",
      DUMMY_DATE_2,
      SUBMISSION_RESULT_FAILED
    )
  );

  crash = s.getCrash("crash1");
  Assert.equal(crash.submissions.size, 1);
  submission = crash.submissions.get("sub1");
  Assert.ok(!!submission);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE.getTime());
  Assert.equal(submission.responseDate.getTime(), DUMMY_DATE_2.getTime());
  Assert.equal(submission.result, SUBMISSION_RESULT_FAILED);

  Assert.ok(s.addSubmissionAttempt("crash1", "sub2", DUMMY_DATE));
  Assert.ok(
    s.addSubmissionResult("crash1", "sub2", DUMMY_DATE_2, SUBMISSION_RESULT_OK)
  );

  Assert.equal(crash.submissions.size, 2);
  submission = crash.submissions.get("sub2");
  Assert.ok(!!submission);
  Assert.equal(submission.result, SUBMISSION_RESULT_OK);
});

add_task(async function test_setCrashClassification() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "crash1", new Date())
  );
  let classifications = s.crashes[0].classifications;
  Assert.ok(!!classifications);
  Assert.equal(classifications.length, 0);

  Assert.ok(s.setCrashClassifications("crash1", ["foo", "bar"]));
  classifications = s.crashes[0].classifications;
  Assert.equal(classifications.length, 2);
  Assert.ok(classifications.includes("foo"));
  Assert.ok(classifications.includes("bar"));
});

add_task(async function test_setRemoteCrashID() {
  let s = await getStore();

  Assert.ok(
    s.addCrash(PROCESS_TYPE_MAIN, CRASH_TYPE_CRASH, "crash1", new Date())
  );
  Assert.equal(s.crashes[0].remoteID, null);
  Assert.ok(s.setRemoteCrashID("crash1", "bp-1"));
  Assert.equal(s.crashes[0].remoteID, "bp-1");
});
