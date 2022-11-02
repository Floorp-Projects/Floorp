/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_backgroundtask_deletes_profile() {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel],
    onStdoutLine: line => stdoutLines.push(line),
  });
  Assert.equal(0, exitCode);

  let profile;
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      profile = info[1];
    }
  }
  Assert.ok(!!profile, `Found profile: ${profile}`);

  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(profile);
  Assert.ok(
    !dir.exists(),
    `Temporary profile directory does not exist: ${profile}`
  );
});

let c = {
  // See note about macOS and temporary directories below.
  skip_if: () => AppConstants.platform == "macosx",
};

add_task(c, async function test_backgroundtask_cleans_up_stale_profiles() {
  // Background tasks shutdown and removal raced with creating files in the
  // temporary profile directory, leaving stale profile remnants on disk.  We
  // try to incrementally clean up such remnants.  This test verifies that clean
  // up succeeds as expected.  In the future we might test that locked profiles
  // are ignored and that a failure during a removal does not stop other
  // removals from being processed, etc.

  // Background task temporary profiles are created in the OS temporary
  // directory.  On Windows, we can put the temporary directory in a
  // testing-specific location by setting TMP, per
  // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw.
  // On Linux, this works as well: see
  // [GetSpecialSystemDirectory](https://searchfox.org/mozilla-central/source/xpcom/io/SpecialSystemDirectory.cpp).
  // On macOS, we can't set the temporary directory in this manner so we skip
  // this test.
  let tmp = do_get_profile();
  tmp.append("Temp");
  tmp.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  // Get the "profile prefix" that the clean up process generates by fishing the
  // profile directory from the testing task that provides the profile path.
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel],
    extraEnv: { TMP: tmp.path },
    onStdoutLine: line => stdoutLines.push(line),
  });
  Assert.equal(0, exitCode);

  let profile;
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      profile = info[1];
    }
  }
  Assert.ok(!!profile, `Found profile: ${profile}`);

  Assert.ok(
    profile.startsWith(tmp.path),
    "Profile was created in test-specific temporary path"
  );

  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(profile);

  // Create a few "stale" profile directories.
  let ps = [];
  for (let i = 0; i < 3; i++) {
    let p = dir.parent.clone();
    p.append(`${dir.leafName}_${i}`);
    p.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);
    ps.push(p);

    let f = p.clone();
    f.append(`file_${i}`);
    await IOUtils.writeJSON(f.path, {});
  }

  // Display logging for ease of debugging.
  let moz_log = "BackgroundTasks:5";
  if (Services.env.exists("MOZ_LOG")) {
    moz_log += `,${Services.env.get("MOZ_LOG")}`;
  }

  // Invoke the task.
  exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel],
    extraEnv: {
      TMP: tmp.path,
      MOZ_LOG: moz_log,
      MOZ_BACKGROUNDTASKS_PURGE_STALE_PROFILES: "always",
    },
  });
  Assert.equal(0, exitCode);

  // Verify none of the stale profile directories persist.
  let es = []; // Expecteds.
  let as = []; // Actuals.
  for (let p of ps) {
    as.push(!p.exists());
    es.push(true);
  }
  Assert.deepEqual(es, as, "All stale profile directories were cleaned up.");
});
