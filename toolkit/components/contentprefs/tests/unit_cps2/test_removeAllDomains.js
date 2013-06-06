/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

let tests = [

  function nonexistent() {
    yield setGlobal("foo", 1);
    yield cps.removeAllDomains(null, makeCallback());
    yield dbOK([
      [null, "foo", 1],
    ]);
    yield getGlobalOK(["foo"], 1);
  },

  function domains() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);
    yield set("b.com", "bar", 6);

    yield cps.removeAllDomains(null, makeCallback());
    yield dbOK([
      [null, "foo", 3],
      [null, "bar", 4],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], 3);
    yield getGlobalOK(["bar"], 4);
    yield getOK(["b.com", "foo"], undefined);
    yield getOK(["b.com", "bar"], undefined);
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
    yield cps.removeAllDomains(context, makeCallback());
    yield dbOK([
      [null, "foo", 3],
      [null, "bar", 4],
    ]);
    yield getOK(["a.com", "foo", context], undefined);
    yield getOK(["a.com", "bar", context], undefined);
    yield getGlobalOK(["foo", context], 7);
    yield getGlobalOK(["bar", context], 4);
    yield getOK(["b.com", "foo", context], undefined);

    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], 3);
    yield getGlobalOK(["bar"], 4);
    yield getOK(["b.com", "foo"], undefined);
  },

  function erroneous() {
    do_check_throws(function () cps.removeAllDomains(null, "bogus"));
    yield true;
  },

  function invalidateCache() {
    yield set("a.com", "foo", 1);
    yield set("b.com", "bar", 2);
    yield setGlobal("baz", 3);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["b.com", "bar"], true, 2);
    getCachedGlobalOK(["baz"], true, 3);
    cps.removeAllDomains(null, makeCallback());
    getCachedOK(["a.com", "foo"], false);
    getCachedOK(["b.com", "bar"], false);
    getCachedGlobalOK(["baz"], true, 3);
    yield;
  },
];
