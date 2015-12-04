/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

var tests = [

  function* get_nonexistent() {
    yield getSubdomainsOK(["a.com", "foo"], []);
  },

  function* isomorphicDomains() {
    yield set("a.com", "foo", 1);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    yield getSubdomainsOK(["http://a.com/huh", "foo"], [["a.com", 1]]);
  },

  function* names() {
    yield set("a.com", "foo", 1);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);

    yield set("a.com", "bar", 2);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    yield getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);

    yield setGlobal("foo", 3);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    yield getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  },

  function* subdomains() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "foo", 2);
    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1], ["b.a.com", 2]]);
    yield getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);
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
    yield getSubdomainsOK(["a.com", "foo", context], [["a.com", 6]]);
    yield getSubdomainsOK(["a.com", "bar", context], [["a.com", 2]]);
    yield getSubdomainsOK(["b.com", "foo", context], [["b.com", 5]]);

    yield getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
    yield getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
    yield getSubdomainsOK(["b.com", "foo"], [["b.com", 5]]);
  },

  function* erroneous() {
    do_check_throws(() => cps.getBySubdomainAndName(null, "foo", null, {}));
    do_check_throws(() => cps.getBySubdomainAndName("", "foo", null, {}));
    do_check_throws(() => cps.getBySubdomainAndName("a.com", "", null, {}));
    do_check_throws(() => cps.getBySubdomainAndName("a.com", null, null, {}));
    do_check_throws(() => cps.getBySubdomainAndName("a.com", "foo", null, null));
    yield true;
  },
];
