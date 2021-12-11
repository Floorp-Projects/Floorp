/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// // This undoes some of the configuration from
// Services.dirsvc
//   .QueryInterface(Ci.nsIDirectoryService)
//   .unregisterProvider(provider);

class PrefObserver {
  constructor() {
    this.events = [];
  }

  onStringPref(...args) {
    // // What happens with an exception here?
    // throw new Error(`foo ${args[1]}`);
    this.events.push(["string", ...args]);
  }

  onIntPref(...args) {
    this.events.push(["int", ...args]);
  }

  onBoolPref(...args) {
    this.events.push(["bool", ...args]);
  }

  onError(message) {
    this.events.push(["error", message]);
  }
}

const TESTS = {
  "data/testPrefSticky.js": [
    ["bool", "Default", "testPref.unsticky.bool", true, false, false],
    ["bool", "Default", "testPref.sticky.bool", false, true, false],
  ],
  "data/testPrefStickyUser.js": [
    ["bool", "User", "testPref.sticky.bool", false, false, false],
  ],
  "data/testPrefLocked.js": [
    ["int", "Default", "testPref.unlocked.int", 333, false, false],
    ["int", "Default", "testPref.locked.int", 444, false, true],
  ],
  // data/testPrefLockedUser is ASCII.
  "data/testPrefLockedUser.js": [
    ["int", "User", "testPref.locked.int", 555, false, false],
  ],
  // data/testPref is ISO-8859.
  "data/testPref.js": [
    ["bool", "User", "testPref.bool1", true, false, false],
    ["bool", "User", "testPref.bool2", false, false, false],
    ["int", "User", "testPref.int1", 23, false, false],
    ["int", "User", "testPref.int2", -1236, false, false],
    ["string", "User", "testPref.char1", "_testPref", false, false],
    ["string", "User", "testPref.char2", "älskar", false, false],
  ],
  // data/testParsePrefs is data/testPref.js as UTF-8.
  "data/testPrefUTF8.js": [
    ["bool", "User", "testPref.bool1", true, false, false],
    ["bool", "User", "testPref.bool2", false, false, false],
    ["int", "User", "testPref.int1", 23, false, false],
    ["int", "User", "testPref.int2", -1236, false, false],
    ["string", "User", "testPref.char1", "_testPref", false, false],
    // Observe that this is the ISO-8859/Latin-1 encoding of the UTF-8 bytes.
    // (Note that this source file is encoded as UTF-8.)  This appears to just
    // be how libpref handles this case.  This test serves as documentation of
    // this infelicity.
    ["string", "User", "testPref.char2", "Ã¤lskar", false, false],
  ],
};

add_task(async function test_success() {
  for (let [path, expected] of Object.entries(TESTS)) {
    let prefObserver = new PrefObserver();

    let prefsFile = do_get_file(path);
    let data = await IOUtils.read(prefsFile.path);

    Services.prefs.parsePrefsFromBuffer(data, prefObserver, path);
    Assert.deepEqual(
      prefObserver.events,
      expected,
      `Observations from ${path} are as expected`
    );
  }
});

add_task(async function test_exceptions() {
  const { AddonTestUtils } = ChromeUtils.import(
    "resource://testing-common/AddonTestUtils.jsm"
  );

  let s = `user_pref("testPref.bool1", true);
  user_pref("testPref.bool2", false);
  user_pref("testPref.int1", 23);
  user_pref("testPref.int2", -1236);
  `;

  let onErrorCount = 0;
  let addPrefCount = 0;

  let marker = "2fc599a1-433b-4de7-bd63-3e69c3bbad5b";
  let addPref = (...args) => {
    addPrefCount += 1;
    throw new Error(`${marker}${JSON.stringify(args)}${marker}`);
  };

  let { messages } = await AddonTestUtils.promiseConsoleOutput(() => {
    Services.prefs.parsePrefsFromBuffer(new TextEncoder().encode(s), {
      onStringPref: addPref,
      onIntPref: addPref,
      onBoolPref: addPref,
      onError(message) {
        onErrorCount += 1;
        console.error(message);
      },
    });
  });

  Assert.equal(addPrefCount, 4);
  Assert.equal(onErrorCount, 0);

  // This is fragile but mercifully simple.
  Assert.deepEqual(
    messages.map(m => JSON.parse(m.message.split(marker)[1])),
    [
      ["User", "testPref.bool1", true, false, false],
      ["User", "testPref.bool2", false, false, false],
      ["User", "testPref.int1", 23, false, false],
      ["User", "testPref.int2", -1236, false, false],
    ]
  );
});
