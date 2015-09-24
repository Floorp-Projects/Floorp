/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function nonexistent() {
    getCachedSubdomainsOK(["a.com", "foo"], []);
    yield true;
  },

  function isomorphicDomains() {
    yield set("a.com", "foo", 1);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    getCachedSubdomainsOK(["http://a.com/huh", "foo"], [["a.com", 1]]);
  },

  function names() {
    yield set("a.com", "foo", 1);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);

    yield set("a.com", "bar", 2);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);

    yield setGlobal("foo", 3);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
    getCachedGlobalOK(["foo"], true, 3);

    yield setGlobal("bar", 4);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
    getCachedGlobalOK(["foo"], true, 3);
    getCachedGlobalOK(["bar"], true, 4);
  },

  function subdomains() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1], ["b.a.com", 2]]);
    getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);
  },

  function populateViaGet() {
    yield cps.getByDomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);

    yield cps.getGlobal("foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);
  },

  function populateViaGetSubdomains() {
    yield cps.getBySubdomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  },

  function populateViaRemove() {
    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);

    yield cps.removeBySubdomainAndName("b.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);

    yield cps.removeGlobal("foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);

    yield set("a.com", "foo", 1);
    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);

    yield set("a.com", "foo", 2);
    yield set("b.a.com", "foo", 3);
    yield cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"],
                          [["a.com", undefined], ["b.a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);
    getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", undefined]]);

    yield setGlobal("foo", 4);
    yield cps.removeGlobal("foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"],
                          [["a.com", undefined], ["b.a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);
    getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", undefined]]);
  },

  function populateViaRemoveByDomain() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield set("b.a.com", "foo", 3);
    yield set("b.a.com", "bar", 4);
    yield cps.removeByDomain("a.com", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"],
                          [["a.com", undefined], ["b.a.com", 3]]);
    getCachedSubdomainsOK(["a.com", "bar"],
                          [["a.com", undefined], ["b.a.com", 4]]);

    yield set("a.com", "foo", 5);
    yield set("a.com", "bar", 6);
    yield cps.removeBySubdomain("a.com", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"],
                          [["a.com", undefined], ["b.a.com", undefined]]);
    getCachedSubdomainsOK(["a.com", "bar"],
                          [["a.com", undefined], ["b.a.com", undefined]]);

    yield setGlobal("foo", 7);
    yield setGlobal("bar", 8);
    yield cps.removeAllGlobals(null, makeCallback());
    getCachedGlobalOK(["foo"], true, undefined);
    getCachedGlobalOK(["bar"], true, undefined);
  },

  function populateViaRemoveAllDomains() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield set("b.com", "foo", 3);
    yield set("b.com", "bar", 4);
    yield cps.removeAllDomains(null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
    getCachedSubdomainsOK(["b.com", "bar"], [["b.com", undefined]]);
  },

  function populateViaRemoveByName() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield cps.removeByName("foo", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
    getCachedGlobalOK(["foo"], true, undefined);
    getCachedGlobalOK(["bar"], true, 4);

    yield cps.removeByName("bar", null, makeCallback());
    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", undefined]]);
    getCachedGlobalOK(["foo"], true, undefined);
    getCachedGlobalOK(["bar"], true, undefined);
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
    getCachedSubdomainsOK(["a.com", "foo", context], [["a.com", 6]]);
    getCachedSubdomainsOK(["a.com", "bar", context], [["a.com", 2]]);
    getCachedGlobalOK(["foo", context], true, 7);
    getCachedGlobalOK(["bar", context], true, 4);
    getCachedSubdomainsOK(["b.com", "foo", context], [["b.com", 5]]);

    getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
    getCachedGlobalOK(["foo"], true, 3);
    getCachedGlobalOK(["bar"], true, 4);
    getCachedSubdomainsOK(["b.com", "foo"], [["b.com", 5]]);
  },

  function erroneous() {
    do_check_throws(() => cps.getCachedBySubdomainAndName(null, "foo", null));
    do_check_throws(() => cps.getCachedBySubdomainAndName("", "foo", null));
    do_check_throws(() => cps.getCachedBySubdomainAndName("a.com", "", null));
    do_check_throws(() => cps.getCachedBySubdomainAndName("a.com", null, null));
    yield true;
  },
];
