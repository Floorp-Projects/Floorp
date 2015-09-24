/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function nonexistent() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);

    yield cps.removeByName("bogus", null, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 2],
    ]);
    yield getOK(["a.com", "foo"], 1);
    yield getGlobalOK(["foo"], 2);
  },

  function names() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);
    yield set("b.com", "bar", 6);

    yield cps.removeByName("foo", null, makeCallback());
    yield dbOK([
      ["a.com", "bar", 2],
      [null, "bar", 4],
      ["b.com", "bar", 6],
    ]);
    yield getOK(["a.com", "foo"], undefined);
    yield getOK(["a.com", "bar"], 2);
    yield getGlobalOK(["foo"], undefined);
    yield getGlobalOK(["bar"], 4);
    yield getOK(["b.com", "foo"], undefined);
    yield getOK(["b.com", "bar"], 6);
  },

  function privateBrowsing() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);
    yield set("b.com", "bar", 6);

    let context = { usePrivateBrowsing: true };
    yield set("a.com", "foo", 7, context);
    yield setGlobal("foo", 8, context);
    yield set("b.com", "bar", 9, context);
    yield cps.removeByName("bar", context, makeCallback());
    yield dbOK([
      ["a.com", "foo", 1],
      [null, "foo", 3],
      ["b.com", "foo", 5],
    ]);
    yield getOK(["a.com", "foo", context], 7);
    yield getOK(["a.com", "bar", context], undefined);
    yield getGlobalOK(["foo", context], 8);
    yield getGlobalOK(["bar", context], undefined);
    yield getOK(["b.com", "foo", context], 5);
    yield getOK(["b.com", "bar", context], undefined);

    yield getOK(["a.com", "foo"], 1);
    yield getOK(["a.com", "bar"], undefined);
    yield getGlobalOK(["foo"], 3);
    yield getGlobalOK(["bar"], undefined);
    yield getOK(["b.com", "foo"], 5);
    yield getOK(["b.com", "bar"], undefined);
  },

  function erroneous() {
    do_check_throws(() => cps.removeByName("", null));
    do_check_throws(() => cps.removeByName(null, null));
    do_check_throws(() => cps.removeByName("foo", null, "bogus"));
    yield true;
  },

  function invalidateCache() {
    yield set("a.com", "foo", 1);
    yield set("b.com", "foo", 2);
    getCachedOK(["a.com", "foo"], true, 1);
    getCachedOK(["b.com", "foo"], true, 2);
    cps.removeByName("foo", null, makeCallback());
    getCachedOK(["a.com", "foo"], false);
    getCachedOK(["b.com", "foo"], false);
    yield;
  },
];
