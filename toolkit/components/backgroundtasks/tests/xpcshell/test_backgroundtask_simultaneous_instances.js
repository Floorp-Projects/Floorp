/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Display logging for ease of debugging.
let moz_log = "BackgroundTasks:5";

// Background task temporary profiles are created in the OS temporary directory.
// On Windows, we can put the temporary directory in a testing-specific location
// by setting TMP, per
// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw.
// On Linux, this works as well: see
// [GetSpecialSystemDirectory](https://searchfox.org/mozilla-central/source/xpcom/io/SpecialSystemDirectory.cpp).
// On macOS, we can't set the temporary directory in this manner, so we will
// leak some temporary directories.  It's important to test this functionality
// on macOS so we accept this.
let tmp = do_get_profile();
tmp.append("Temp");

add_setup(() => {
  if (Services.env.exists("MOZ_LOG")) {
    moz_log += `,${Services.env.get("MOZ_LOG")}`;
  }

  tmp.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o700);
});

async function launch_one(index) {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel],
    extraEnv: {
      MOZ_BACKGROUNDTASKS_NO_REMOVE_PROFILE: "1",
      MOZ_LOG: moz_log,
      TMP: tmp.path, // Ignored on macOS.
    },
    onStdoutLine: line => stdoutLines.push(line),
  });

  let profile;
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      profile = info[1];
    }
  }

  return { index, sentinel, exitCode, profile };
}

add_task(async function test_backgroundtask_simultaneous_instances() {
  let expectedCount = 10;
  let promises = [];

  for (let i = 0; i < expectedCount; i++) {
    promises.push(launch_one(i));
  }

  let results = await Promise.all(promises);

  registerCleanupFunction(() => {
    for (let result of results) {
      if (!result.profile) {
        return;
      }

      let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      dir.initWithPath(result.profile);
      try {
        dir.remove();
      } catch (e) {
        console.warn("Could not clean up profile directory", e);
      }
    }
  });

  for (let result of results) {
    Assert.equal(
      0,
      result.exitCode,
      `Invocation ${result.index} with sentinel ${result.sentinel} exited with code 0`
    );
    Assert.ok(
      result.profile,
      `Invocation ${result.index} with sentinel ${result.sentinel} yielded a temporary profile directory`
    );
  }

  let profiles = new Set(results.map(it => it.profile));
  Assert.equal(
    expectedCount,
    profiles.size,
    `Invocations used ${expectedCount} different temporary profile directories`
  );
});
