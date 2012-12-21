/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

let tests = [

  function observerForName_set() {
    let args = on("Set", ["foo", null, "bar"]);

    yield set("a.com", "foo", 1);
    observerArgsOK(args.foo, [["a.com", "foo", 1]]);
    observerArgsOK(args.null, [["a.com", "foo", 1]]);
    observerArgsOK(args.bar, []);
    args.reset();

    yield setGlobal("foo", 2);
    observerArgsOK(args.foo, [[null, "foo", 2]]);
    observerArgsOK(args.null, [[null, "foo", 2]]);
    observerArgsOK(args.bar, []);
    args.reset();
  },

  function observerForName_remove() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);

    let args = on("Removed", ["foo", null, "bar"]);
    yield cps.removeByDomainAndName("a.com", "bogus", null, makeCallback());
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();

    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"]]);
    observerArgsOK(args.bar, []);
    args.reset();

    yield cps.removeGlobal("foo", null, makeCallback());
    observerArgsOK(args.foo, [[null, "foo"]]);
    observerArgsOK(args.null, [[null, "foo"]]);
    observerArgsOK(args.bar, []);
    args.reset();
  },

  function observerForName_removeByDomain() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "bar", 2);
    yield setGlobal("foo", 3);

    let args = on("Removed", ["foo", null, "bar"]);
    yield cps.removeByDomain("bogus", null, makeCallback());
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();

    yield cps.removeBySubdomain("a.com", null, makeCallback());
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], ["b.a.com", "bar"]]);
    observerArgsOK(args.bar, [["b.a.com", "bar"]]);
    args.reset();

    yield cps.removeAllGlobals(null, makeCallback());
    observerArgsOK(args.foo, [[null, "foo"]]);
    observerArgsOK(args.null, [[null, "foo"]]);
    observerArgsOK(args.bar, []);
    args.reset();
  },

  function observerForName_removeAllDomains() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);
    yield set("b.com", "bar", 3);

    let args = on("Removed", ["foo", null, "bar"]);
    yield cps.removeAllDomains(null, makeCallback());
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], ["b.com", "bar"]]);
    observerArgsOK(args.bar, [["b.com", "bar"]]);
    args.reset();
  },

  function observerForName_removeByName() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);

    let args = on("Removed", ["foo", null, "bar"]);
    yield cps.removeByName("bogus", null, makeCallback());
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();

    yield cps.removeByName("foo", null, makeCallback());
    observerArgsOK(args.foo, [["a.com", "foo"], [null, "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], [null, "foo"]]);
    observerArgsOK(args.bar, []);
    args.reset();
  },

  function removeObserverForName() {
    let args = on("Set", ["foo", null, "bar"]);

    cps.removeObserverForName("foo", args.foo.observer);
    yield set("a.com", "foo", 1);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, [["a.com", "foo", 1]]);
    observerArgsOK(args.bar, []);
    args.reset();

    cps.removeObserverForName(null, args.null.observer);
    yield set("a.com", "foo", 2);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();
  },
];
