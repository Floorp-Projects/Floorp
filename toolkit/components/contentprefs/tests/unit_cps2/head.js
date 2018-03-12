/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

let loadContext = Cc["@mozilla.org/loadcontext;1"].
                    createInstance(Ci.nsILoadContext);
let privateLoadContext = Cc["@mozilla.org/privateloadcontext;1"].
                           createInstance(Ci.nsILoadContext);

// There has to be a profile directory before the CPS service is gotten.
do_get_profile();
let cps = Cc["@mozilla.org/content-pref/service;1"].
          getService(Ci.nsIContentPrefService2);

function makeCallback(resolve, callbacks, success = null) {
  callbacks = callbacks || {};
  if (!callbacks.handleError) {
    callbacks.handleError = function(error) {
      do_throw("handleError call was not expected, error: " + error);
    };
  }
  if (!callbacks.handleResult) {
    callbacks.handleResult = function() {
      do_throw("handleResult call was not expected");
    };
  }
  if (!callbacks.handleCompletion)
    callbacks.handleCompletion = function(reason) {
      equal(reason, Ci.nsIContentPrefCallback2.COMPLETE_OK);
      if (success) {
        success();
      } else {
        resolve();
      }
    };
  return callbacks;
}

async function do_check_throws(fn) {
  let threw = false;
  try {
    await fn();
  } catch (err) {
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
  return new Promise(resolve => {
    sendMessage("reset", resolve);
  });
}

function setWithDate(group, name, val, timestamp, context) {
  return new Promise(resolve => {
    async function updateDate() {
      let conn = await sendMessage("db");
      await conn.execute(`
        UPDATE prefs SET timestamp = :timestamp
        WHERE
          settingID = (SELECT id FROM settings WHERE name = :name)
          AND groupID = (SELECT id FROM groups WHERE name = :group)
      `, {name, group, timestamp: timestamp / 1000});

      resolve();
    }

    cps.set(group, name, val, context, makeCallback(null, null, updateDate));
  });
}

async function getDate(group, name, context) {
  let conn = await sendMessage("db");
  let [result] = await conn.execute(`
      SELECT timestamp FROM prefs
      WHERE
        settingID = (SELECT id FROM settings WHERE name = :name)
        AND groupID = (SELECT id FROM groups WHERE name = :group)
  `, {name, group});

  return result.getResultByName("timestamp") * 1000;
}

function set(group, name, val, context) {
  return new Promise(resolve => {
    cps.set(group, name, val, context, makeCallback(resolve));
  });
}

function setGlobal(name, val, context) {
  return new Promise(resolve => {
    cps.setGlobal(name, val, context, makeCallback(resolve));
  });
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

async function getOK(args, expectedVal, expectedGroup, strict) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: expectedGroup || args[0],
                         name: args[1],
                         value: expectedVal }];
  await getOKEx("getByDomainAndName", args, expectedPrefs, strict);
}

async function getSubdomainsOK(args, expectedGroupValPairs) {
  if (args.length == 2)
    args.push(undefined);
  let expectedPrefs = expectedGroupValPairs.map(function([group, val]) {
    return { domain: group, name: args[1], value: val };
  });
  await getOKEx("getBySubdomainAndName", args, expectedPrefs);
}

async function getGlobalOK(args, expectedVal) {
  if (args.length == 1)
    args.push(undefined);
  let expectedPrefs = expectedVal === undefined ? [] :
                      [{ domain: null, name: args[0], value: expectedVal }];
  await getOKEx("getGlobal", args, expectedPrefs);
}

async function getOKEx(methodName, args, expectedPrefs, strict, context) {
  let actualPrefs = [];
  await new Promise(resolve => {
    args.push(makeCallback(resolve, {
      handleResult: pref => actualPrefs.push(pref)
    }));
    cps[methodName].apply(cps, args);
  });
  arraysOfArraysOK([actualPrefs], [expectedPrefs], function(actual, expected) {
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
  actualPrefs = actualPrefs.sort(function(a, b) {
    return a.domain.localeCompare(b.domain);
  });
  equal(actualPrefs.length, len.value);
  let expectedPrefs = expectedGroupValPairs.map(function([group, val]) {
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
    actual.forEach(function(actualElt, j) {
      let expectedElt = expected[j];
      cmp(actualElt, expectedElt);
    });
  }
}

function arraysOfArraysOK(actual, expected, cmp) {
  cmp = cmp || equal;
  arraysOK(actual, expected, function(act, exp) {
    arraysOK(act, exp, cmp);
  });
}

async function dbOK(expectedRows) {
  let conn = await sendMessage("db");
  let stmt = `
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
  `;

  let cols = ["grp", "name", "value"];

  let actualRows = (await conn.execute(stmt)).map(row => (cols.map(c => row.getResultByName(c))));
  arraysOfArraysOK(actualRows, expectedRows);
}

function on(event, names, dontRemove) {
  return onEx(event, names, dontRemove).promise;
}

function onEx(event, names, dontRemove) {
  let args = {
    reset() {
      for (let prop in this) {
        if (Array.isArray(this[prop]))
          this[prop].splice(0, this[prop].length);
      }
    },
  };

  let observers = {};
  let deferred = null;
  let triggered = false;

  names.forEach(function(name) {
    let obs = {};
    ["onContentPrefSet", "onContentPrefRemoved"].forEach(function(meth) {
      obs[meth] = () => do_throw(meth + " should not be called for " + name);
    });
    obs["onContentPref" + event] = function() {
      args[name].push(Array.slice(arguments));

      if (!triggered) {
        triggered = true;
        executeSoon(function() {
          if (!dontRemove)
            names.forEach(n => cps.removeObserverForName(n, observers[n]));
          deferred.resolve(args);
        });
      }
    };
    observers[name] = obs;
    args[name] = [];
    args[name].observer = obs;
    cps.addObserverForName(name, obs);
  });

  return {
    observers,
    promise: new Promise(resolve => {
      deferred = {resolve};
    }),
  };
}

async function schemaVersionIs(expectedVersion) {
  let db = await sendMessage("db");
  equal(await db.getSchemaVersion(), expectedVersion);
}

function wait() {
  return new Promise(resolve => executeSoon(resolve));
}

function observerArgsOK(actualArgs, expectedArgs) {
  notEqual(actualArgs, undefined);
  arraysOfArraysOK(actualArgs, expectedArgs);
}
