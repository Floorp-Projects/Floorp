/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function get_nonexistent() {
  await getOK(["a.com", "foo"], undefined);
  await getGlobalOK(["foo"], undefined);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  await dbOK([
    ["a.com", "foo", 1],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getOK(["http://a.com/huh", "foo"], 1, "a.com");

  await set("http://a.com/huh", "foo", 2);
  await dbOK([
    ["a.com", "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 2);
  await getOK(["http://a.com/yeah", "foo"], 2, "a.com");
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  await dbOK([
    ["a.com", "foo", 1],
  ]);
  await getOK(["a.com", "foo"], 1);

  await set("a.com", "bar", 2);
  await dbOK([
    ["a.com", "foo", 1],
    ["a.com", "bar", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], 2);

  await setGlobal("foo", 3);
  await dbOK([
    ["a.com", "foo", 1],
    ["a.com", "bar", 2],
    [null, "foo", 3],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], 3);

  await setGlobal("bar", 4);
  await dbOK([
    ["a.com", "foo", 1],
    ["a.com", "bar", 2],
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  await dbOK([
    ["a.com", "foo", 1],
    ["b.a.com", "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getOK(["b.a.com", "foo"], 2);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);

  let context = privateLoadContext;
  await set("a.com", "foo", 6, context);
  await setGlobal("foo", 7, context);
  await dbOK([
    ["a.com", "foo", 1],
    ["a.com", "bar", 2],
    [null, "foo", 3],
    [null, "bar", 4],
    ["b.com", "foo", 5],
  ]);
  await getOK(["a.com", "foo", context], 6, "a.com");
  await getOK(["a.com", "bar", context], 2);
  await getGlobalOK(["foo", context], 7);
  await getGlobalOK(["bar", context], 4);
  await getOK(["b.com", "foo", context], 5);

  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], 5);
  await reset();
});

add_task(async function set_erroneous() {
  do_check_throws(() => cps.set(null, "foo", 1, null));
  do_check_throws(() => cps.set("", "foo", 1, null));
  do_check_throws(() => cps.set("a.com", "", 1, null));
  do_check_throws(() => cps.set("a.com", null, 1, null));
  do_check_throws(() => cps.set("a.com", "foo", undefined, null));
  do_check_throws(() => cps.set("a.com", "foo", 1, null, "bogus"));
  do_check_throws(() => cps.setGlobal("", 1, null));
  do_check_throws(() => cps.setGlobal(null, 1, null));
  do_check_throws(() => cps.setGlobal("foo", undefined, null));
  do_check_throws(() => cps.setGlobal("foo", 1, null, "bogus"));
  await reset();
});

add_task(async function get_erroneous() {
  do_check_throws(() => cps.getByDomainAndName(null, "foo", null, {}));
  do_check_throws(() => cps.getByDomainAndName("", "foo", null, {}));
  do_check_throws(() => cps.getByDomainAndName("a.com", "", null, {}));
  do_check_throws(() => cps.getByDomainAndName("a.com", null, null, {}));
  do_check_throws(() => cps.getByDomainAndName("a.com", "foo", null, null));
  do_check_throws(() => cps.getGlobal("", null, {}));
  do_check_throws(() => cps.getGlobal(null, null, {}));
  do_check_throws(() => cps.getGlobal("foo", null, null));
  await reset();
});

add_task(async function set_invalidateCache() {
  // (1) Set a pref and wait for it to finish.
  await set("a.com", "foo", 1);

  // (2) It should be cached.
  getCachedOK(["a.com", "foo"], true, 1);

  // (3) Set the pref to a new value but don't wait for it to finish.
  cps.set("a.com", "foo", 2, null, {
    handleCompletion() {
      // (6) The pref should be cached after setting it.
      getCachedOK(["a.com", "foo"], true, 2);
    },
  });

  // (4) Group "a.com" and name "foo" should no longer be cached.
  getCachedOK(["a.com", "foo"], false);

  // (5) Call getByDomainAndName.
  let fetchedPref;
  let getPromise = new Promise(resolve => cps.getByDomainAndName("a.com", "foo", null, {
    handleResult(pref) {
      fetchedPref = pref;
    },
    handleCompletion() {
      // (7) Finally, this callback should be called after set's above.
      Assert.ok(!!fetchedPref);
      Assert.equal(fetchedPref.value, 2);
      resolve();
    },
  }));

  await getPromise;
  await reset();
});

add_task(async function get_nameOnly() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await set("b.com", "foo", 3);
  await setGlobal("foo", 4);

  await getOKEx("getByName", ["foo", undefined], [
    {"domain": "a.com", "name": "foo", "value": 1},
    {"domain": "b.com", "name": "foo", "value": 3},
    {"domain": null, "name": "foo", "value": 4}
  ]);

  let context = privateLoadContext;
  await set("b.com", "foo", 5, context);

  await getOKEx("getByName", ["foo", context], [
    {"domain": "a.com", "name": "foo", "value": 1},
    {"domain": null, "name": "foo", "value": 4},
    {"domain": "b.com", "name": "foo", "value": 5}
  ]);
  await reset();
});

add_task(async function setSetsCurrentDate() {
  // Because Date.now() is not guaranteed to be monotonically increasing
  // we just do here rough sanity check with one minute tolerance.
  const MINUTE = 60 * 1000;
  let now = Date.now();
  let start = now - MINUTE;
  let end = now + MINUTE;
  await set("a.com", "foo", 1);
  let timestamp = await getDate("a.com", "foo");
  ok(start <= timestamp, "Timestamp is not too early (" + start + "<=" + timestamp + ").");
  ok(timestamp <= end, "Timestamp is not too late (" + timestamp + "<=" + end + ").");
  await reset();
});
