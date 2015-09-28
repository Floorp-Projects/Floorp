/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function nonexistent() {
    getCachedOK(["a.com", "foo"], false, undefined);
    getCachedGlobalOK(["foo"], false, undefined);
    yield true;
  },

  function isomorphicDomains() {
    yield set("a.com", "foo", 1);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["http://a.com/huh", "foo"], true, 1, "a.com");
  },

  function names() {
    yield set("a.com", "foo", 1);
    getCachedOK(["a.com", "foo"], true, 1);

    yield set("a.com", "bar", 2);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["a.com", "bar"], true, 2);

    yield setGlobal("foo", 3);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["a.com", "bar"], true, 2);
    getCachedGlobalOK(["foo"], true, 3);

    yield setGlobal("bar", 4);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["a.com", "bar"], true, 2);
    getCachedGlobalOK(["foo"], true, 3);
    getCachedGlobalOK(["bar"], true, 4);
  },

  function subdomains() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["b.a.com", "foo"], true, 2);
  },

  function privateBrowsing() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);

    let context = { usePrivateBrowsing: true };
    yield set("a.com", "foo", 6, context);
    yield setGlobal("foo", 7, context);
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
  },

  function erroneous() {
    do_check_throws(() => cps.getCachedByDomainAndName(null, "foo", null));
    do_check_throws(() => cps.getCachedByDomainAndName("", "foo", null));
    do_check_throws(() => cps.getCachedByDomainAndName("a.com", "", null));
    do_check_throws(() => cps.getCachedByDomainAndName("a.com", null, null));
    do_check_throws(() => cps.getCachedGlobal("", null));
    do_check_throws(() => cps.getCachedGlobal(null, null));
    yield true;
  },

  function casts() {
    // SQLite casts booleans to integers.  This makes sure the values stored in
    // the cache are the same as the casted values in the database.

    yield set("a.com", "foo", false);
    yield getOK(["a.com", "foo"], 0, "a.com", true);
    getCachedOK(["a.com", "foo"], true, 0, "a.com", true);

    yield set("a.com", "bar", true);
    yield getOK(["a.com", "bar"], 1, "a.com", true);
    getCachedOK(["a.com", "bar"], true, 1, "a.com", true);
  },
];
