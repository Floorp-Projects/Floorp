/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function* nonexistent() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);

    yield cps.removeByDomain("bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);

    yield cps.removeBySubdomain("bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);
  },

  function* isomorphicDomains() {
    yield set("a.com", "foo", 1);
    yield cps.removeByDomain("a.com", null, makeCallback());
    yield dbOK([]);
    yield getOK(["a.com", "foo"], undefined);

    yield set("a.com", "foo", 2);
    yield cps.removeByDomain("http://a.com/huh", null, makeCallback());
    yield dbOK([]);
    yield getOK(["a.com", "foo"], undefined);
  },

  function* domains() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);
    yield set("b.com", "bar", 6);

    yield cps.removeByDomain("a.com", null, makeCallback());
    yield dbOK([
      [null, "foo", 3],
      [null, "bar", 4],
      ["b.com", "foo", 5],
      ["b.com", "bar", 6],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], 3);
    yield getGlobalOK(["bar"], 4);
    yield getOK(["b.com", "foo"], 5);
    yield getOK(["b.com", "bar"], 6);

    yield cps.removeAllGlobals(null, makeCallback());
    yield dbOK([
      ["b.com", "foo", 5],
      ["b.com", "bar", 6],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], undefined);
    yield getOK(["b.com", "foo"], 5);
    yield getOK(["b.com", "bar"], 6);

    yield cps.removeByDomain("b.com", null, makeCallback());
    yield dbOK([
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], undefined);
    yield getOK(["b.com", "foo"], undefined);
    yield getOK(["b.com", "bar"], undefined);
  },

  function* subdomains() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    yield cps.removeByDomain("a.com", null, makeCallback());
    yield dbOK([
      ["b.a.com", "foo", 2],
    ]);
    yield getSubdomainsOK(["a.com", "foo"], [["b.a.com", 2]]);
    yield getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);

    yield set("a.com", "foo", 3);
    yield cps.removeBySubdomain("a.com", null, makeCallback());
    yield dbOK([
    ]);
    yield getSubdomainsOK(["a.com", "foo"], []);
    yield getSubdomainsOK(["b.a.com", "foo"], []);

    yield set("a.com", "foo", 4);
    yield set("b.a.com", "foo", 5);
    yield cps.removeByDomain("b.a.com", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 4],
    ]);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
    yield getSubdomainsOK(["b.a.com", "foo"], []);

    yield set("b.a.com", "foo", 6);
    yield cps.removeBySubdomain("b.a.com", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 4],
    ]);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
    yield getSubdomainsOK(["b.a.com", "foo"], []);
  },

  function* privateBrowsing() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);

    let context = { usePrivateBrowsing: true };
    yield set("a.com", "foo", 6, context);
    yield set("b.com", "foo", 7, context);
    yield setGlobal("foo", 8, context);
    yield cps.removeByDomain("a.com", context, makeCallback());
    yield getOK(["b.com", "foo", context], 7);
    yield getGlobalOK(["foo", context], 8);
    yield cps.removeAllGlobals(context, makeCallback());
    yield dbOK([
      ["b.com", "foo", 5],
    ]);
    yield getOK(["a.com", "foo", context], undefined);
    yield getOK(["a.com", "bar", context], undefined);
    yield getGlobalOK(["foo", context], undefined);
    yield getGlobalOK(["bar", context], undefined);
    yield getOK(["b.com", "foo", context], 5);

    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], undefined);
    yield getOK(["b.com", "foo"], 5);
  },

  function* erroneous() {
    do_check_throws(() => cps.removeByDomain(null, null));
    do_check_throws(() => cps.removeByDomain("", null));
    do_check_throws(() => cps.removeByDomain("a.com", null, "bogus"));
    do_check_throws(() => cps.removeBySubdomain(null, null));
    do_check_throws(() => cps.removeBySubdomain("", null));
    do_check_throws(() => cps.removeBySubdomain("a.com", null, "bogus"));
    do_check_throws(() => cps.removeAllGlobals(null, "bogus"));
    yield true;
  },

  function* removeByDomain_invalidateCache() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["a.com", "bar"], true, 2);
    cps.removeByDomain("a.com", null, makeCallback());
    getCachedOK(["a.com", "foo"], false);
    getCachedOK(["a.com", "bar"], false);
    yield;
  },

  function* removeBySubdomain_invalidateCache() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    getCachedSubdomainsOK(["a.com", "foo"], [
      ["a.com", 1],
      ["b.a.com", 2],
    ]);
    cps.removeBySubdomain("a.com", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], []);
    yield;
  },

  function* removeAllGlobals_invalidateCache() {
    yield setGlobal("foo", 1);
    yield setGlobal("bar", 2);
    getCachedGlobalOK(["foo"], true, 1);
    getCachedGlobalOK(["bar"], true, 2);
    cps.removeAllGlobals(null, makeCallback());
    getCachedGlobalOK(["foo"], false);
    getCachedGlobalOK(["bar"], false);
    yield;
  },
];
