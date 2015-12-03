/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function* nonexistent() {
    yield setGlobal("foo", 1);
    yield cps.removeAllDomainsSince(0, null, makeCallback());
    yield getGlobalOK(["foo"], 1);
  },

  function* domainsAll() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);
    yield set("b.com", "bar", 6);

    yield cps.removeAllDomainsSince(0, null, makeCallback());
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

  function* domainsWithDate() {
    yield setWithDate("a.com", "foobar", 0, 0);
    yield setWithDate("a.com", "foo", 1, 1000);
    yield setWithDate("a.com", "bar", 2, 4000);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield setWithDate("b.com", "foo", 5, 2000);
    yield setWithDate("b.com", "bar", 6, 3000);
    yield setWithDate("b.com", "foobar", 7, 1000);

    yield cps.removeAllDomainsSince(2000, null, makeCallback());
    yield dbOK([
      ["a.com", "foobar", 0],
      ["a.com", "foo", 1],
      [null, "foo", 3],
      [null, "bar", 4],
      ["b.com", "foobar", 7],
    ]);
  },

  function* privateBrowsing() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield set("b.com", "foo", 5);

    let context = { usePrivateBrowsing: true };
    yield set("a.com", "foo", 6, context);
    yield setGlobal("foo", 7, context);
    yield cps.removeAllDomainsSince(0, context, makeCallback());
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

  function* erroneous() {
    do_check_throws(() => cps.removeAllDomainsSince(null, "bogus"));
    yield true;
  },

  function* invalidateCache() {
    yield setWithDate("a.com", "foobar", 0, 0);
    yield setWithDate("a.com", "foo", 1, 1000);
    yield setWithDate("a.com", "bar", 2, 4000);
    yield setGlobal("foo", 3);
    yield setGlobal("bar", 4);
    yield setWithDate("b.com", "foo", 5, 2000);
    yield setWithDate("b.com", "bar", 6, 3000);
    yield setWithDate("b.com", "foobar", 7, 1000);
    cps.removeAllDomainsSince(0, null, makeCallback());
    getCachedOK(["a.com", "foobar"], false);
    getCachedOK(["a.com", "foo"], false);
    getCachedOK(["a.com", "bar"], false);
    getCachedGlobalOK(["foo"], true, 3);
    getCachedGlobalOK(["bar"], true, 4);
    getCachedOK(["b.com", "foo"], false);
    getCachedOK(["b.com", "bar"], false);
    getCachedOK(["b.com", "foobar"], false);
    yield true;
  },
];
