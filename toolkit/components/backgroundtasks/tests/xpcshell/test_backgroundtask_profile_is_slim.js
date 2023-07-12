/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Invoke `profile_is_slim` background task, have it *not* remove its
// temporary profile, and verify that the temporary profile doesn't
// contain unexpected files and/or directories.
//
// N.b.: the background task invocation is a production Firefox
// running in automation; that means it can only connect to localhost
// (and it is not configured to connect to mochi.test, etc).
// Therefore we run our own server for it to connect to.

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
let server;

setupProfileService();

let successCount = 0;
function success(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let str = "success";
  response.write(str, str.length);
  successCount += 1;
}

add_setup(() => {
  server = new HttpServer();
  server.registerPathHandler("/success", success);
  server.start(-1);

  registerCleanupFunction(async () => {
    await server.stop();
  });
});

add_task(async function test_backgroundtask_profile_is_slim() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profD = do_get_profile();
  profD.append("test_profile_is_slim");
  let profile = profileService.createUniqueProfile(
    profD,
    "test_profile_is_slim"
  );

  registerCleanupFunction(() => {
    profile.remove(true);
  });

  let exitCode = await do_backgroundtask("profile_is_slim", {
    extraArgs: [`http://localhost:${server.identity.primaryPort}/success`],
    extraEnv: {
      XRE_PROFILE_PATH: profile.rootDir.path,
      MOZ_BACKGROUNDTASKS_NO_REMOVE_PROFILE: "1",
    },
  });

  Assert.equal(
    0,
    exitCode,
    "The fetch background task exited with exit code 0"
  );
  Assert.equal(
    1,
    successCount,
    "The fetch background task reached the test server 1 time"
  );

  assertProfileIsSlim(profile.rootDir);
});

const expected = [
  { relPath: "lock", condition: AppConstants.platform == "linux" },
  {
    relPath: ".parentlock",
    condition:
      AppConstants.platform == "linux" || AppConstants.platform == "macosx",
  },
  {
    relPath: "parent.lock",
    condition:
      AppConstants.platform == "win" || AppConstants.platform == "macosx",
  },
  // TODO: verify that directory is empty.
  { relPath: "cache2", isDirectory: true },
  { relPath: "compatibility.ini" },
  { relPath: "crashes", isDirectory: true },
  { relPath: "data", isDirectory: true },
  { relPath: "local", isDirectory: true },
  { relPath: "minidumps", isDirectory: true },
  // `mozinfo.json` is an artifact of the testing infrastructure.
  { relPath: "mozinfo.json" },
  { relPath: "pkcs11.txt" },
  { relPath: "prefs.js" },
  { relPath: "security_state", isDirectory: true },
  // TODO: verify that directory is empty.
  { relPath: "startupCache", isDirectory: true },
  { relPath: "times.json" },
];

async function assertProfileIsSlim(profile) {
  Assert.ok(profile.exists(), `Profile directory does exist: ${profile.path}`);

  // We collect unexpected results, log them all, and then assert
  // there are none to provide the most feedback per iteration.
  let unexpected = [];

  let typeLabel = { true: "directory", false: "file" };

  for (let entry of profile.directoryEntries) {
    // `getRelativePath` always uses '/' as path separator.
    let relPath = entry.getRelativePath(profile);

    info(`Witnessed directory entry '${relPath}'.`);

    let expectation = expected.find(
      it => it.relPath == relPath && (!("condition" in it) || it.condition)
    );
    if (!expectation) {
      info(`UNEXPECTED: Directory entry '${relPath}' is NOT expected!`);
      unexpected.push(relPath);
    } else if (
      typeLabel[!!expectation.isDirectory] != typeLabel[entry.isDirectory()]
    ) {
      info(`UNEXPECTED: Directory entry '${relPath}' is NOT expected type!`);
      unexpected.push(relPath);
    }
  }

  Assert.deepEqual([], unexpected, "Expected nothing to report");
}
