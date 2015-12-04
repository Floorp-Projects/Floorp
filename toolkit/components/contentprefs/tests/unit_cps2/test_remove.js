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

    yield cps.removeByDomainAndName("a.com", "bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);

    yield cps.removeBySubdomainAndName("a.com", "bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);

    yield cps.removeGlobal("bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);

    yield cps.removeByDomainAndName("bogus", "bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);
  },

  function* isomorphicDomains() {
    yield set("a.com", "foo", 1);
    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    yield dbOK([]);
    yield getOK(["a.com", "foo"], undefined);

    yield set("a.com", "foo", 2);
    yield cps.removeByDomainAndName("http://a.com/huh", "foo", null,
                                    makeCallback());
    yield dbOK([]);
    yield getOK(["a.com", "foo"], undefined);
  },

  function* names() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);

    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    yield dbOK([
      ["a.com", "bar", 2],
      [null, "foo", 3],
      [null, "bar", 4],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], 2);
    yield getGlobalOK(["foo"], 3);
    yield getGlobalOK(["bar"], 4);

    yield cps.removeGlobal("foo", null, makeCallback());
    yield dbOK([
      ["a.com", "bar", 2],
      [null, "bar", 4],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], 2);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], 4);

    yield cps.removeByDomainAndName("a.com", "bar", null, makeCallback());
    yield dbOK([
      [null, "bar", 4],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], 4);

    yield cps.removeGlobal("bar", null, makeCallback());
    yield dbOK([
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], undefined);
  },

  function* subdomains() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    yield dbOK([
      ["b.a.com", "foo", 2],
    ]);
    yield getSubdomainsOK(["a.com", "foo"], [["b.a.com", 2]]);
    yield getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);

    yield set("a.com", "foo", 3);
    yield cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback());
    yield dbOK([
    ]);
    yield getSubdomainsOK(["a.com", "foo"], []);
    yield getSubdomainsOK(["b.a.com", "foo"], []);

    yield set("a.com", "foo", 4);
    yield set("b.a.com", "foo", 5);
    yield cps.removeByDomainAndName("b.a.com", "foo", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 4],
    ]);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
    yield getSubdomainsOK(["b.a.com", "foo"], []);

    yield set("b.a.com", "foo", 6);
    yield cps.removeBySubdomainAndName("b.a.com", "foo", null, makeCallback());
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
    yield setGlobal("qux", 5);
    yield set("b.com", "foo", 6);
    yield set("b.com", "bar", 7);

    let context = { usePrivateBrowsing: true };
    yield set("a.com", "foo", 8, context);
    yield setGlobal("foo", 9, context);
    yield cps.removeByDomainAndName("a.com", "foo", context, makeCallback());
    yield cps.removeGlobal("foo", context, makeCallback());
    yield cps.removeGlobal("qux", context, makeCallback());
    yield cps.removeByDomainAndName("b.com", "foo", context, makeCallback());
    yield dbOK([
      ["a.com", "bar", 2],
      [null, "bar", 4],
      ["b.com", "bar", 7],
    ]);
    yield getOK(["a.com", "foo", context], undefined);
    yield getOK(["a.com", "bar", context], 2);
    yield getGlobalOK(["foo", context], undefined);
    yield getGlobalOK(["bar", context], 4);
    yield getGlobalOK(["qux", context], undefined);
    yield getOK(["b.com", "foo", context], undefined);
    yield getOK(["b.com", "bar", context], 7);

    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], 2);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], 4);
    yield getGlobalOK(["qux"], undefined);
    yield getOK(["b.com", "foo"], undefined);
    yield getOK(["b.com", "bar"], 7);
  },

  function* erroneous() {
    do_check_throws(() => cps.removeByDomainAndName(null, "foo", null));
    do_check_throws(() => cps.removeByDomainAndName("", "foo", null));
    do_check_throws(() => cps.removeByDomainAndName("a.com", "foo", null,
                                                    "bogus"));
    do_check_throws(() => cps.removeBySubdomainAndName(null, "foo",
                                                       null));
    do_check_throws(() => cps.removeBySubdomainAndName("", "foo", null));
    do_check_throws(() => cps.removeBySubdomainAndName("a.com", "foo",
                                                       null, "bogus"));
    do_check_throws(() => cps.removeGlobal("", null));
    do_check_throws(() => cps.removeGlobal(null, null));
    do_check_throws(() => cps.removeGlobal("foo", null, "bogus"));
    yield true;
  },

  function* removeByDomainAndName_invalidateCache() {
    yield set("a.com", "foo", 1);
    getCachedOK(["a.com", "foo"], true, 1);
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    getCachedOK(["a.com", "foo"], false);
    yield;
  },

  function* removeBySubdomainAndName_invalidateCache() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    getCachedSubdomainsOK(["a.com", "foo"], [
      ["a.com", 1],
      ["b.a.com", 2],
    ]);
    cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], []);
    yield;
  },

  function* removeGlobal_invalidateCache() {
    yield setGlobal("foo", 1);
    getCachedGlobalOK(["foo"], true, 1);
    cps.removeGlobal("foo", null, makeCallback());
    getCachedGlobalOK(["foo"], false);
    yield;
  },
];
