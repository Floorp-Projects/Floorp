/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  getCachedOK(["a.com", "foo"], false, undefined);
  getCachedGlobalOK(["foo"], false, undefined);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["http://a.com/huh", "foo"], true, 1, "a.com");
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  getCachedOK(["a.com", "foo"], true, 1);

  await set("a.com", "bar", 2);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["a.com", "bar"], true, 2);

  await setGlobal("foo", 3);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["a.com", "bar"], true, 2);
  getCachedGlobalOK(["foo"], true, 3);

  await setGlobal("bar", 4);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["a.com", "bar"], true, 2);
  getCachedGlobalOK(["foo"], true, 3);
  getCachedGlobalOK(["bar"], true, 4);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["b.a.com", "foo"], true, 2);
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
  getCachedOK(["a.com", "foo", context], true, 6);
  getCachedOK(["a.com", "bar", context], true, 2);
  getCachedGlobalOK(["foo", context], true, 7);
  getCachedGlobalOK(["bar", context], true, 4);
  getCachedOK(["b.com", "foo", context], true, 5);

  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["a.com", "bar"], true, 2);
  getCachedGlobalOK(["foo"], true, 3);
  getCachedGlobalOK(["bar"], true, 4);
  getCachedOK(["b.com", "foo"], true, 5);
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.getCachedByDomainAndName(null, "foo", null));
  do_check_throws(() => cps.getCachedByDomainAndName("", "foo", null));
  do_check_throws(() => cps.getCachedByDomainAndName("a.com", "", null));
  do_check_throws(() => cps.getCachedByDomainAndName("a.com", null, null));
  do_check_throws(() => cps.getCachedGlobal("", null));
  do_check_throws(() => cps.getCachedGlobal(null, null));
  await reset();
});

add_task(async function casts() {
  // SQLite casts booleans to integers.  This makes sure the values stored in
  // the cache are the same as the casted values in the database.

  await set("a.com", "foo", false);
  await getOK(["a.com", "foo"], 0, "a.com", true);
  getCachedOK(["a.com", "foo"], true, 0, "a.com", true);

  await set("a.com", "bar", true);
  await getOK(["a.com", "bar"], 1, "a.com", true);
  getCachedOK(["a.com", "bar"], true, 1, "a.com", true);
  await reset();
});
