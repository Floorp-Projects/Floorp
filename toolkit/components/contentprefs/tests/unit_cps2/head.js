/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { interfaces: Ci, classes: Cc, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

var cps;
var asyncRunner;
var next;

(function init() {
  // There has to be a profile directory before the CPS service is gotten.
  do_get_profile();
})();

function runAsyncTests(tests, dontResetBefore = false) {
  do_test_pending();

  cps = Cc["@mozilla.org/content-pref/service;1"].
        getService(Ci.nsIContentPrefService2);

  let s = {};
  Cu.import("resource://test/AsyncRunner.jsm", s);
  asyncRunner = new s.AsyncRunner({
    done: do_test_finished,
    error: function (err) {
      // xpcshell test functions like equal throw NS_ERROR_ABORT on
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
      // Previously, this code checked for console errors related to the test,
      // and treated them as failures. This was problematic, because our current
      // very-broken exception reporting machinery in XPCWrappedJSClass reports
      // errors to the console even if there's actually JS on the stack above
      // that will catch them. And a lot of the tests here intentionally trigger
      // error conditions on the JS-implemented XPCOM component (see erroneous()
      // in test_getSubdomains.js, for example). In the old world, we got lucky,
      // and the errors were never reported to the console due to happenstantial
      // JSContext reasons that aren't really worth going into.
      //
      // So. We make sure to dump this stuff so that it shows up in the logs, but
      // don't turn them into duplicate failures of the exception that was already
      // propagated to the caller.
      dump("AsyncRunner.jsm observed console error: " +  scriptErr + "\n");
    }
  });

  next = asyncRunner.next.bind(asyncRunner);

  do_register_cleanup(function () {
    asyncRunner.destroy();
    asyncRunner = null;
  });

  tests.forEach(function (test) {
    function* gen() {
      do_print("Running " + test.name);
      yield test();
      yield reset();
    }
    asyncRunner.appendIterator(gen());
  });

  // reset() ends up calling asyncRunner.next(), starting the tests.
  if (dontResetBefore) {
    next();
  } else {
    reset();
  }
}

function makeCallback(callbacks, success = null) {
  callbacks = callbacks || {};
  if (!callbacks.handleError) {
    callbacks.handleError = function (error) {
      do_throw("handleError call was not expected, error: " + error);
    };
  }
  if (!callbacks.handleResult) {
    callbacks.handleResult = function() {
      do_throw("handleResult call was not expected");
    };
  }
  if (!callbacks.handleCompletion)
    callbacks.handleCompletion = function (reason) {
      equal(reason, Ci.nsIContentPrefCallback2.COMPLETE_OK);
      if (success) {
        success();
      } else {
        next();
      }
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
  ok(threw);
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

function setWithDate(group, name, val, timestamp, context) {
  function updateDate() {
    let db = sendMessage("db");
    let stmt = db.createAsyncStatement(`
      UPDATE prefs SET timestamp = :timestamp
      WHERE
        settingID = (SELECT id FROM settings WHERE name = :name)
        AND groupID = (SELECT id FROM groups WHERE name = :group)
    `);
    stmt.params.timestamp = timestamp / 1000;
    stmt.params.name = name;
    stmt.params.group = group;

    stmt.executeAsync({
      handleCompletion: function (reason) {
        next();
      },
      handleError: function (err) {
        do_throw(err);
      }
    });
    stmt.finalize();
  }

  cps.set(group, name, val, context, makeCallback(null, updateDate));
}

function getDate(group, name, context) {
  let db = sendMessage("db");
  let stmt = db.createAsyncStatement(`
    SELECT timestamp FROM prefs
    WHERE
      settingID = (SELECT id FROM settings WHERE name = :name)
      AND groupID = (SELECT id FROM groups WHERE name = :group)
  `);
  stmt.params.name = name;
  stmt.params.group = group;

  let res;
  stmt.executeAsync({
    handleResult: function (results) {
      let row = results.getNextRow();
      res = row.getResultByName("timestamp");
    },
    handleCompletion: function (reason) {
      next(res * 1000);
    },
    handleError: function (err) {
      do_throw(err);
    }
  });
  stmt.finalize();
}

function set(group, name, val, context) {
  cps.set(group, name, val, context, makeCallback());
}

function setGlobal(name, val, context) {
  cps.setGlobal(name, val, context, makeCallback());
}

function prefOK(actual, expected, strict) {
  ok(actual instanceof Ci.nsIContentPref);
  equal(actual.domain, expected.domain);
  equal(actual.name, expected.name);
  if (strict)
    strictEqual(actual.value, expected.value);
  else
    equal(actual.value, expected.value);
}

function* getOK(args, expectedVal, expectedGroup, strict) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: expectedGroup || args[0],
                         name: args[1],
                         value: expectedVal }];
  yield getOKEx("getByDomainAndName", args, expectedPrefs, strict);
}

function* getSubdomainsOK(args, expectedGroupValPairs) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedGroupValPairs.map(function ([group, val]) {
    return { domain: group, name: args[1], value: val };
  });
  yield getOKEx("getBySubdomainAndName", args, expectedPrefs);
}

function* getGlobalOK(args, expectedVal) {
  if (args.length == 1)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: null, name: args[0], value: expectedVal }];
  yield getOKEx("getGlobal", args, expectedPrefs);
}

function* getOKEx(methodName, args, expectedPrefs, strict, context) {
  let actualPrefs = [];
  args.push(makeCallback({
    handleResult: pref => actualPrefs.push(pref)
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
  actualPrefs = actualPrefs.sort(function (a, b) {
    return a.domain.localeCompare(b.domain);
  });
  equal(actualPrefs.length, len.value);
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
    strictEqual(actualPref, null);
}

function arraysOK(actual, expected, cmp) {
  if (actual.length != expected.length) {
    do_throw("Length is not equal: " + JSON.stringify(actual) + "==" + JSON.stringify(expected));
  } else {
    actual.forEach(function (actualElt, j) {
      let expectedElt = expected[j];
      cmp(actualElt, expectedElt);
    });
  }
}

function arraysOfArraysOK(actual, expected, cmp) {
  cmp = cmp || equal;
  arraysOK(actual, expected, function (act, exp) {
    arraysOK(act, exp, cmp)
  });
}

function dbOK(expectedRows) {
  let db = sendMessage("db");
  let stmt = db.createAsyncStatement(`
    SELECT groups.name AS grp, settings.name AS name, prefs.value AS value
    FROM prefs
    LEFT JOIN groups ON groups.id = prefs.groupID
    LEFT JOIN settings ON settings.id = prefs.settingID
    UNION

    /*
      These second two SELECTs get the rows of the groups and settings tables
      that aren't referenced by the prefs table.  Neither should return any
      rows if the component is working properly.
    */
    SELECT groups.name AS grp, NULL AS name, NULL AS value
    FROM groups
    WHERE id NOT IN (
      SELECT DISTINCT groupID
      FROM prefs
      WHERE groupID NOTNULL
    )
    UNION
    SELECT NULL AS grp, settings.name AS name, NULL AS value
    FROM settings
    WHERE id NOT IN (
      SELECT DISTINCT settingID
      FROM prefs
      WHERE settingID NOTNULL
    )

    ORDER BY value ASC, grp ASC, name ASC
  `);

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
        actualRows.push(cols.map(c => row.getResultByName(c)));
      }
    },
    handleError: function (err) {
      do_throw(err);
    }
  });
  stmt.finalize();
}

function on(event, names, dontRemove) {
  let args = {
    reset: function () {
      for (let prop in this) {
        if (Array.isArray(this[prop]))
          this[prop].splice(0, this[prop].length);
      }
    },
  };

  let observers = {};

  names.forEach(function (name) {
    let obs = {};
    ["onContentPrefSet", "onContentPrefRemoved"].forEach(function (meth) {
      obs[meth] = () => do_throw(meth + " should not be called");
    });
    obs["onContentPref" + event] = function () {
      args[name].push(Array.slice(arguments));
    };
    observers[name] = obs;
    args[name] = [];
    args[name].observer = obs;
    cps.addObserverForName(name, obs);
  });

  do_execute_soon(function () {
    if (!dontRemove)
      names.forEach(n => cps.removeObserverForName(n, observers[n]));
    next(args);
  });
}

function schemaVersionIs(expectedVersion) {
  let db = sendMessage("db");
  equal(db.schemaVersion, expectedVersion);
}

function wait() {
  do_execute_soon(next);
}

function observerArgsOK(actualArgs, expectedArgs) {
  notEqual(actualArgs, undefined);
  arraysOfArraysOK(actualArgs, expectedArgs);
}
