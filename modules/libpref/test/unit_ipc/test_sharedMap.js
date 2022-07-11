/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file tests the functionality of the preference service when using a
// shared memory snapshot. In this configuration, a snapshot of the initial
// state of the preferences database is made when we first spawn a child
// process, and changes after that point are stored as entries in a dynamic hash
// table, on top of the snapshot.

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

let contentPage;

const { prefs } = Services;
const defaultPrefs = prefs.getDefaultBranch("");

const FRAME_SCRIPT_INIT = `
  var { prefs } = Services;
  var defaultPrefs = prefs.getDefaultBranch("");
`;

function try_(fn) {
  try {
    return fn();
  } catch (e) {
    return undefined;
  }
}

function getPref(pref) {
  let flags = {
    locked: try_(() => prefs.prefIsLocked(pref)),
    hasUser: try_(() => prefs.prefHasUserValue(pref)),
  };

  switch (prefs.getPrefType(pref)) {
    case prefs.PREF_INT:
      return {
        ...flags,
        type: "Int",
        user: try_(() => prefs.getIntPref(pref)),
        default: try_(() => defaultPrefs.getIntPref(pref)),
      };
    case prefs.PREF_BOOL:
      return {
        ...flags,
        type: "Bool",
        user: try_(() => prefs.getBoolPref(pref)),
        default: try_(() => defaultPrefs.getBoolPref(pref)),
      };
    case prefs.PREF_STRING:
      return {
        ...flags,
        type: "String",
        user: try_(() => prefs.getStringPref(pref)),
        default: try_(() => defaultPrefs.getStringPref(pref)),
      };
  }
  return {};
}

function getPrefs(prefNames) {
  let result = {};
  for (let pref of prefNames) {
    result[pref] = getPref(pref);
  }
  result.childList = prefs.getChildList("");
  return result;
}

function checkPref(
  pref,
  proc,
  val,
  type,
  userVal,
  defaultVal,
  expectedFlags = {}
) {
  info(`Check "${pref}" ${proc} value`);

  equal(val.type, type, `Expected type for "${pref}"`);
  equal(val.user, userVal, `Expected user value for "${pref}"`);

  // We only send changes to the content process when they'll make a visible
  // difference, so ignore content process default values when we have a defined
  // user value.
  if (proc !== "content" || val.user === undefined) {
    equal(val.default, defaultVal, `Expected default value for "${pref}"`);
  }

  for (let [flag, value] of Object.entries(expectedFlags)) {
    equal(val[flag], value, `Expected ${flag} value for "${pref}"`);
  }
}

function getPrefList() {
  return prefs.getChildList("");
}

const TESTS = {
  "exists.thenDoesNot": {
    beforeContent(PREF) {
      prefs.setBoolPref(PREF, true);

      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
    },
    contentStartup(PREF, val, childList) {
      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
      ok(childList.includes(PREF), `Child list includes "${PREF}"`);

      prefs.clearUserPref(PREF);
      ok(
        !getPrefList().includes(PREF),
        `Parent list doesn't include "${PREF}"`
      );
    },
    contentUpdate1(PREF, val, childList) {
      ok(
        !getPrefList().includes(PREF),
        `Parent list doesn't include "${PREF}"`
      );
      ok(!childList.includes(PREF), `Child list doesn't include "${PREF}"`);

      prefs.setCharPref(PREF, "foo");
      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
      checkPref(PREF, "parent", getPref(PREF), "String", "foo");
    },
    contentUpdate2(PREF, val, childList) {
      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
      ok(childList.includes(PREF), `Child list includes "${PREF}"`);

      checkPref(PREF, "parent", getPref(PREF), "String", "foo");
      checkPref(PREF, "child", val, "String", "foo");
    },
  },
  "doesNotExists.thenDoes": {
    contentStartup(PREF, val, childList) {
      ok(
        !getPrefList().includes(PREF),
        `Parent list doesn't include "${PREF}"`
      );
      ok(!childList.includes(PREF), `Child list doesn't include "${PREF}"`);

      prefs.setIntPref(PREF, 42);
      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
    },
    contentUpdate1(PREF, val, childList) {
      ok(getPrefList().includes(PREF), `Parent list includes "${PREF}"`);
      ok(childList.includes(PREF), `Child list includes "${PREF}"`);

      checkPref(PREF, "parent", getPref(PREF), "Int", 42);
      checkPref(PREF, "child", val, "Int", 42);
    },
  },
};

const PREFS = [
  { type: "Bool", values: [true, false, true] },
  { type: "Int", values: [24, 42, 73] },
  { type: "String", values: ["meh", "hem", "hrm"] },
];

for (let { type, values } of PREFS) {
  let set = `set${type}Pref`;

  function prefTest(opts) {
    function check(
      pref,
      proc,
      val,
      {
        expectedVal,
        defaultVal = undefined,
        expectedDefault = defaultVal,
        expectedFlags = {},
      }
    ) {
      checkPref(
        pref,
        proc,
        val,
        type,
        expectedVal,
        expectedDefault,
        expectedFlags
      );
    }

    function updatePref(
      PREF,
      { userVal = undefined, defaultVal = undefined, flags = {} }
    ) {
      info(`Update "${PREF}"`);
      if (userVal !== undefined) {
        prefs[set](PREF, userVal);
      }
      if (defaultVal !== undefined) {
        defaultPrefs[set](PREF, defaultVal);
      }
      if (flags.locked === true) {
        prefs.lockPref(PREF);
      } else if (flags.locked === false) {
        prefs.unlockPref(PREF);
      }
    }

    return {
      beforeContent(PREF) {
        updatePref(PREF, opts.initial);
        check(PREF, "parent", getPref(PREF), opts.initial);
      },
      contentStartup(PREF, contentVal) {
        check(PREF, "content", contentVal, opts.initial);
        check(PREF, "parent", getPref(PREF), opts.initial);

        updatePref(PREF, opts.change1);
        check(PREF, "parent", getPref(PREF), opts.change1);
      },
      contentUpdate1(PREF, contentVal) {
        check(PREF, "content", contentVal, opts.change1);
        check(PREF, "parent", getPref(PREF), opts.change1);

        if (opts.change2) {
          updatePref(PREF, opts.change2);
          check(PREF, "parent", getPref(PREF), opts.change2);
        }
      },
      contentUpdate2(PREF, contentVal) {
        if (opts.change2) {
          check(PREF, "content", contentVal, opts.change2);
          check(PREF, "parent", getPref(PREF), opts.change2);
        }
      },
    };
  }

  for (let i of [0, 1]) {
    let userVal = values[i];
    let defaultVal = values[+!i];

    TESTS[`type.${type}.${i}.default`] = prefTest({
      initial: { defaultVal, expectedVal: defaultVal },
      change1: { defaultVal: values[2], expectedVal: values[2] },
    });

    TESTS[`type.${type}.${i}.user`] = prefTest({
      initial: { userVal, expectedVal: userVal },
      change1: { defaultVal: values[2], expectedVal: userVal },
      change2: {
        userVal: values[2],
        expectedDefault: values[2],
        expectedVal: values[2],
      },
    });

    TESTS[`type.${type}.${i}.both`] = prefTest({
      initial: { userVal, defaultVal, expectedVal: userVal },
      change1: { defaultVal: values[2], expectedVal: userVal },
      change2: {
        userVal: values[2],
        expectedDefault: values[2],
        expectedVal: values[2],
      },
    });

    TESTS[`type.${type}.${i}.both.thenLock`] = prefTest({
      initial: { userVal, defaultVal, expectedVal: userVal },
      change1: {
        expectedDefault: defaultVal,
        expectedVal: defaultVal,
        flags: { locked: true },
        expectFlags: { locked: true },
      },
    });

    TESTS[`type.${type}.${i}.both.thenUnlock`] = prefTest({
      initial: {
        userVal,
        defaultVal,
        expectedVal: defaultVal,
        flags: { locked: true },
        expectedFlags: { locked: true },
      },
      change1: {
        expectedDefault: defaultVal,
        expectedVal: userVal,
        flags: { locked: false },
        expectFlags: { locked: false },
      },
    });

    TESTS[`type.${type}.${i}.both.locked`] = prefTest({
      initial: {
        userVal,
        defaultVal,
        expectedVal: defaultVal,
        flags: { locked: true },
        expectedFlags: { locked: true },
      },
      change1: {
        userVal: values[2],
        expectedDefault: defaultVal,
        expectedVal: defaultVal,
        expectedFlags: { locked: true },
      },
      change2: {
        defaultVal: values[2],
        expectedDefault: defaultVal,
        expectedVal: defaultVal,
        expectedFlags: { locked: true },
      },
    });
  }
}

add_task(async function test_sharedMap_prefs() {
  let prefValues = {};

  async function runChecks(op) {
    for (let [pref, ops] of Object.entries(TESTS)) {
      if (ops[op]) {
        info(`Running ${op} for "${pref}"`);
        await ops[op](
          pref,
          prefValues[pref] || undefined,
          prefValues.childList || undefined
        );
      }
    }
  }

  await runChecks("beforeContent");

  contentPage = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });
  registerCleanupFunction(() => contentPage.close());

  contentPage.addFrameScriptHelper(FRAME_SCRIPT_INIT);
  contentPage.addFrameScriptHelper(try_);
  contentPage.addFrameScriptHelper(getPref);

  let prefNames = Object.keys(TESTS);
  prefValues = await contentPage.spawn(prefNames, getPrefs);

  await runChecks("contentStartup");

  prefValues = await contentPage.spawn(prefNames, getPrefs);

  await runChecks("contentUpdate1");

  prefValues = await contentPage.spawn(prefNames, getPrefs);

  await runChecks("contentUpdate2");
});
