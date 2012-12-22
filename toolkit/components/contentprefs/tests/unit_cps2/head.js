/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, classes: Cc, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

var cps;
var asyncRunner;
var next;

(function init() {
  // There has to be a profile directory before the CPS service is gotten.
  do_get_profile();
})();

function runAsyncTests(tests) {
  do_test_pending();

  cps = Cc["@mozilla.org/content-pref/service;1"].
        getService(Ci.nsIContentPrefService2);

  // Without this the private-browsing service tries to open a dialog when you
  // change its enabled state.
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session",
                             true);

  let s = {};
  Cu.import("resource://test/AsyncRunner.jsm", s);
  asyncRunner = new s.AsyncRunner({
    done: do_test_finished,
    error: function (err) {
      // xpcshell test functions like do_check_eq throw NS_ERROR_ABORT on
      // failure.  Ignore those and catch only uncaught exceptions.
      if (err !== Cr.NS_ERROR_ABORT) {
        if (err.stack) {
          err = err + "\n\nTraceback (most recent call first):\n" + err.stack +
                      "\nUseless do_throw stack:";
        }
        do_throw(err);
      }
    },
    consoleError: function (scriptErr) {
      // As much as possible make sure the error is related to the test.  On the
      // other hand if this fails to catch a test-related error, we'll hang.
      let filename = scriptErr.sourceName || scriptErr.toString() || "";
      if (/contentpref/i.test(filename))
        do_throw(scriptErr);
    }
  });

  next = asyncRunner.next.bind(asyncRunner);

  do_register_cleanup(function () {
    asyncRunner.destroy();
    asyncRunner = null;
  });

  tests.forEach(function (test) {
    function gen() {
      do_print("Running " + test.name);
      yield test();
      yield reset();
    }
    asyncRunner.appendIterator(gen());
  });

  // reset() ends up calling asyncRunner.next(), starting the tests.
  reset();
}

function makeCallback(callbacks) {
  callbacks = callbacks || {};
  ["handleResult", "handleError"].forEach(function (meth) {
    if (!callbacks[meth])
      callbacks[meth] = function () {
        do_throw(meth + " shouldn't be called.");
      };
  });
  if (!callbacks.handleCompletion)
    callbacks.handleCompletion = function (reason) {
      do_check_eq(reason, Ci.nsIContentPrefCallback2.COMPLETE_OK);
      next();
    };
  return callbacks;
}

function do_check_throws(fn) {
  let threw = false;
  try {
    fn();
  }
  catch (err) {
    threw = true;
  }
  do_check_true(threw);
}

function sendMessage(msg, callback) {
  let obj = callback || {};
  let ref = Cu.getWeakReference(obj);
  cps.QueryInterface(Ci.nsIObserver).observe(ref, "test:" + msg, null);
  return "value" in obj ? obj.value : undefined;
}

function reset() {
  sendMessage("reset", next);
}

function set(group, name, val, context) {
  cps.set(group, name, val, context, makeCallback());
}

function setGlobal(name, val, context) {
  cps.setGlobal(name, val, context, makeCallback());
}

function prefOK(actual, expected, strict) {
  do_check_true(actual instanceof Ci.nsIContentPref);
  do_check_eq(actual.domain, expected.domain);
  do_check_eq(actual.name, expected.name);
  if (strict)
    do_check_true(actual.value === expected.value);
  else
    do_check_eq(actual.value, expected.value);
}

function getOK(args, expectedVal, expectedGroup, strict) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: expectedGroup || args[0],
                         name: args[1],
                         value: expectedVal }];
  yield getOKEx("getByDomainAndName", args, expectedPrefs, strict);
}

function getSubdomainsOK(args, expectedGroupValPairs) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedGroupValPairs.map(function ([group, val]) {
    return { domain: group, name: args[1], value: val };
  });
  yield getOKEx("getBySubdomainAndName", args, expectedPrefs);
}

function getGlobalOK(args, expectedVal) {
  if (args.length == 1)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: null, name: args[0], value: expectedVal }];
  yield getOKEx("getGlobal", args, expectedPrefs);
}

function getOKEx(methodName, args, expectedPrefs, strict, context) {
  let actualPrefs = [];
  args.push(makeCallback({
    handleResult: function (pref) actualPrefs.push(pref)
  }));
  yield cps[methodName].apply(cps, args);
  arraysOfArraysOK([actualPrefs], [expectedPrefs], function (actual, expected) {
    prefOK(actual, expected, strict);
  });
}

function getCachedOK(args, expectedIsCached, expectedVal, expectedGroup,
                     strict) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPref = !expectedIsCached ? null : {
    domain: expectedGroup || args[0],
    name: args[1],
    value: expectedVal
  };
  getCachedOKEx("getCachedByDomainAndName", args, expectedPref, strict);
}

function getCachedSubdomainsOK(args, expectedGroupValPairs) {
  if (args.length == 2)
    args.push(undefined);
  let len = {};
  args.push(len);
  let actualPrefs = cps.getCachedBySubdomainAndName.apply(cps, args);
  do_check_eq(actualPrefs.length, len.value);
  let expectedPrefs = expectedGroupValPairs.map(function ([group, val]) {
    return { domain: group, name: args[1], value: val };
  });
  arraysOfArraysOK([actualPrefs], [expectedPrefs], prefOK);
}

function getCachedGlobalOK(args, expectedIsCached, expectedVal) {
  if (args.length == 1)
    args.push(undefined);
  let expectedPref = !expectedIsCached ? null : {
    domain: null,
    name: args[0],
    value: expectedVal
  };
  getCachedOKEx("getCachedGlobal", args, expectedPref);
}

function getCachedOKEx(methodName, args, expectedPref, strict) {
  let actualPref = cps[methodName].apply(cps, args);
  if (expectedPref)
    prefOK(actualPref, expectedPref, strict);
  else
    do_check_true(actualPref === null);
}

function arraysOfArraysOK(actual, expected, cmp) {
  cmp = cmp || function (a, b) do_check_eq(a, b);
  do_check_eq(actual.length, expected.length);
  actual.forEach(function (actualChildArr, i) {
    let expectedChildArr = expected[i];
    do_check_eq(actualChildArr.length, expectedChildArr.length);
    actualChildArr.forEach(function (actualElt, j) {
      let expectedElt = expectedChildArr[j];
      cmp(actualElt, expectedElt);
    });
  });
}

function dbOK(expectedRows) {
  let db = sendMessage("db");
  let stmt = db.createAsyncStatement(
    "SELECT groups.name AS grp, settings.name AS name, prefs.value AS value " +
    "FROM prefs " +
    "LEFT JOIN groups ON groups.id = prefs.groupID " +
    "LEFT JOIN settings ON settings.id = prefs.settingID " +
    "UNION " +

    // These second two SELECTs get the rows of the groups and settings tables
    // that aren't referenced by the prefs table.  Neither should return any
    // rows if the component is working properly.
    "SELECT groups.name AS grp, NULL AS name, NULL AS value " +
    "FROM groups " +
    "WHERE id NOT IN (" +
      "SELECT DISTINCT groupID " +
      "FROM prefs " +
      "WHERE groupID NOTNULL" +
    ") " +
    "UNION " +
    "SELECT NULL AS grp, settings.name AS name, NULL AS value " +
    "FROM settings " +
    "WHERE id NOT IN (" +
      "SELECT DISTINCT settingID " +
      "FROM prefs " +
      "WHERE settingID NOTNULL" +
    ") " +

    "ORDER BY value ASC, grp ASC, name ASC"
  );

  let actualRows = [];
  let cols = ["grp", "name", "value"];

  db.executeAsync([stmt], 1, {
    handleCompletion: function (reason) {
      arraysOfArraysOK(actualRows, expectedRows);
      next();
    },
    handleResult: function (results) {
      let row = null;
      while (row = results.getNextRow()) {
        actualRows.push(cols.map(function (c) row.getResultByName(c)));
      }
    },
    handleError: function (err) {
      do_throw(err);
    }
  });
  stmt.finalize();
}

function on(event, names) {
  let args = {
    reset: function () {
      for (let prop in this) {
        if (Array.isArray(this[prop]))
          this[prop].splice(0, this[prop].length);
      }
    },
    destroy: function () {
      names.forEach(function (n) cps.removeObserverForName(n, observers[n]));
    },
  };

  let observers = {};

  names.forEach(function (name) {
    let obs = {};
    ["onContentPrefSet", "onContentPrefRemoved"].forEach(function (meth) {
      obs[meth] = function () do_throw(meth + " should not be called");
    });
    obs["onContentPref" + event] = function () {
      args[name].push(Array.slice(arguments));
    };
    observers[name] = obs;
    args[name] = [];
    args[name].observer = obs;
    cps.addObserverForName(name, obs);
  });

  return args;
}

function observerArgsOK(actualArgs, expectedArgs) {
  do_check_neq(actualArgs, undefined);
  arraysOfArraysOK(actualArgs, expectedArgs);
}
