/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  Assert.equal(
    kATP.lookupModuleType(kExtensionModuleName),
    0,
    "lookupModuleType() returns 0 before system info is collected."
  );

  // Make sure successive calls of collectSystemInfo() do not
  // cause anything bad.
  const kLoopCount = 100;
  const promises = [];
  for (let i = 0; i < kLoopCount; ++i) {
    promises.push(kATP.collectSystemInfo());
  }

  const collectSystemInfoResults = await Promise.allSettled(promises);
  Assert.equal(collectSystemInfoResults.length, kLoopCount);

  for (const result of collectSystemInfoResults) {
    Assert.ok(
      result.status == "fulfilled",
      "All results from collectSystemInfo() are resolved."
    );
  }

  Assert.equal(
    kATP.lookupModuleType("SHELL32.dll"),
    Ci.nsIAboutThirdParty.ModuleType_ShellExtension,
    "Shell32.dll is always registered as a shell extension."
  );

  Assert.equal(
    kATP.lookupModuleType(""),
    Ci.nsIAboutThirdParty.ModuleType_Unknown,
    "Looking up an empty string succeeds and returns ModuleType_Unknown."
  );

  Assert.equal(
    kATP.lookupModuleType(null),
    Ci.nsIAboutThirdParty.ModuleType_Unknown,
    "Looking up null succeeds and returns ModuleType_Unknown."
  );

  Assert.equal(
    kATP.lookupModuleType("invalid name"),
    Ci.nsIAboutThirdParty.ModuleType_Unknown,
    "Looking up an invalid name succeeds and returns ModuleType_Unknown."
  );

  Assert.equal(
    kATP.lookupApplication(""),
    null,
    "Looking up an empty string returns null."
  );

  Assert.equal(
    kATP.lookupApplication("invalid path"),
    null,
    "Looking up an invalid path returns null."
  );
});
