/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/CrashManager.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

let DUMMY_DIR_COUNT = 0;

function getManager() {
  function mkdir(f) {
    if (f.exists()) {
      return;
    }

    dump("Creating directory: " + f.path + "\n");
    f.create(Ci.nsIFile.DIRECTORY_TYPE, dirMode);
  }

  const dirMode = OS.Constants.libc.S_IRWXU;
  let baseFile = do_get_tempdir();
  let pendingD = baseFile.clone();
  let submittedD = baseFile.clone();
  pendingD.append("dummy-dir-" + DUMMY_DIR_COUNT++);
  submittedD.append("dummy-dir-" + DUMMY_DIR_COUNT++);
  mkdir(pendingD);
  mkdir(submittedD);

  let m = new CrashManager({
    pendingDumpsDir: pendingD.path,
    submittedDumpsDir: submittedD.path,
  });

  m.create_dummy_dump = function (submitted=false, date=new Date(), hr=false) {
    let uuid = Cc["@mozilla.org/uuid-generator;1"]
                 .getService(Ci.nsIUUIDGenerator)
                 .generateUUID()
                 .toString();
    uuid = uuid.substring(1, uuid.length - 1);

    let file;
    let mode;
    if (submitted) {
      file = submittedD.clone();
      if (hr) {
        file.append("bp-hr-" + uuid + ".txt");
      } else {
        file.append("bp-" + uuid + ".txt");
      }
      mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR |
             OS.Constants.libc.S_IRGRP | OS.Constants.libc.S_IROTH;
    } else {
      file = pendingD.clone();
      file.append(uuid + ".dmp");
      mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
    }

    file.create(file.NORMAL_FILE_TYPE, mode);
    file.lastModifiedTime = date.getTime();
    dump("Created fake crash: " + file.path + "\n");

    return uuid;
  };

  m.create_ignored_dump_file = function (filename, submitted=false) {
    let file;
    if (submitted) {
      file = submittedD.clone();
    } else {
      file = pendingD.clone();
    }

    file.append(filename);
    file.create(file.NORMAL_FILE_TYPE,
                OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR);
    dump("Created ignored dump file: " + file.path + "\n");
  };

  return m;
}

function run_test() {
  run_next_test();
}

add_task(function* test_constructor_ok() {
  let m = new CrashManager({
    pendingDumpsDir: "/foo",
    submittedDumpsDir: "/bar",
  });
  Assert.ok(m);
});

add_task(function* test_constructor_invalid() {
  Assert.throws(() => {
    new CrashManager({foo: true});
  });
});

add_task(function* test_get_manager() {
  let m = getManager();
  Assert.ok(m);

  m.create_dummy_dump(true);
  m.create_dummy_dump(false);
});

add_task(function* test_pending_dumps() {
  let m = getManager();
  let now = Date.now();
  let ids = [];
  const COUNT = 5;

  for (let i = 0; i < COUNT; i++) {
    ids.push(m.create_dummy_dump(false, new Date(now - i * 86400000)));
  }
  m.create_ignored_dump_file("ignored", false);

  let entries = yield m.pendingDumps();
  Assert.equal(entries.length, COUNT, "proper number detected.");

  for (let entry of entries) {
    Assert.equal(typeof(entry), "object", "entry is an object");
    Assert.ok("id" in entry, "id in entry");
    Assert.ok("path" in entry, "path in entry");
    Assert.ok("date" in entry, "date in entry");
    Assert.notEqual(ids.indexOf(entry.id), -1, "ID is known");
  }

  for (let i = 0; i < COUNT; i++) {
    Assert.equal(entries[i].id, ids[COUNT-i-1], "Entries sorted by mtime");
  }
});

add_task(function* test_submitted_dumps() {
  let m = getManager();
  let COUNT = 5;

  for (let i = 0; i < COUNT; i++) {
    m.create_dummy_dump(true);
  }
  m.create_ignored_dump_file("ignored", true);

  let entries = yield m.submittedDumps();
  Assert.equal(entries.length, COUNT, "proper number detected.");

  let hrID = m.create_dummy_dump(true, new Date(), true);
  entries = yield m.submittedDumps();
  Assert.equal(entries.length, COUNT + 1, "hr- in filename detected.");

  let gotIDs = new Set([e.id for (e of entries)]);
  Assert.ok(gotIDs.has(hrID));
});

add_task(function* test_submitted_and_pending() {
  let m = getManager();
  let pendingIDs = [];
  let submittedIDs = [];

  pendingIDs.push(m.create_dummy_dump(false));
  pendingIDs.push(m.create_dummy_dump(false));
  submittedIDs.push(m.create_dummy_dump(true));

  let submitted = yield m.submittedDumps();
  let pending = yield m.pendingDumps();

  Assert.equal(submitted.length, submittedIDs.length);
  Assert.equal(pending.length, pendingIDs.length);
});
