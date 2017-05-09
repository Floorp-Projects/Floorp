/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let global = this;

function run_test() {
  var allTests = [];
  for (var i = 0; i < tests.length; i++) {
    // Generate two wrappers of each test function that invoke the original test with an
    // appropriate privacy context.
    /* eslint-disable no-eval */
    var pub = eval("var f = function* " + tests[i].name + "() { yield tests[" + i + "](privateLoadContext); }; f");
    var priv = eval("var f = function* " + tests[i].name + "_private() { yield tests[" + i + "](privateLoadContext); }; f");
    /* eslint-enable no-eval */
    allTests.push(pub);
    allTests.push(priv);
  }
  allTests = allTests.concat(specialTests);
  runAsyncTests(allTests);
}

var tests = [

  function* observerForName_set(context) {
    yield set("a.com", "foo", 1, context);
    let args = yield on("Set", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", 1, context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", 1, context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);

    yield setGlobal("foo", 2, context);
    args = yield on("Set", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo", 2, context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", 2, context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
  },

  function* observerForName_remove(context) {
    yield set("a.com", "foo", 1, context);
    yield setGlobal("foo", 2, context);

    yield cps.removeByDomainAndName("a.com", "bogus", context, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeByDomainAndName("a.com", "foo", context, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);

    yield cps.removeGlobal("foo", context, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
  },

  function* observerForName_removeByDomain(context) {
    yield set("a.com", "foo", 1, context);
    yield set("b.a.com", "bar", 2, context);
    yield setGlobal("foo", 3, context);

    yield cps.removeByDomain("bogus", context, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeBySubdomain("a.com", context, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", context.usePrivateBrowsing], ["b.a.com", "bar", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, [["b.a.com", "bar", context.usePrivateBrowsing]]);

    yield cps.removeAllGlobals(context, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
  },

  function* observerForName_removeAllDomains(context) {
    yield set("a.com", "foo", 1, context);
    yield setGlobal("foo", 2, context);
    yield set("b.com", "bar", 3, context);

    yield cps.removeAllDomains(context, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", context.usePrivateBrowsing], ["b.com", "bar", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, [["b.com", "bar", context.usePrivateBrowsing]]);
  },

  function* observerForName_removeByName(context) {
    yield set("a.com", "foo", 1, context);
    yield set("a.com", "bar", 2, context);
    yield setGlobal("foo", 3, context);

    yield cps.removeByName("bogus", context, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeByName("foo", context, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing], [null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", context.usePrivateBrowsing], [null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
  },

  function* removeObserverForName(context) {
    let args = yield on("Set", ["foo", null, "bar"], true);

    cps.removeObserverForName("foo", args.foo.observer);
    yield set("a.com", "foo", 1, context);
    yield wait();
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, [["a.com", "foo", 1, context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
    args.reset();

    cps.removeObserverForName(null, args.null.observer);
    yield set("a.com", "foo", 2, context);
    yield wait();
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();
  },
];

// These tests are for functionality that doesn't behave the same way in private and public
// contexts, so the expected results cannot be automatically generated like the previous tests.
var specialTests = [
  function* observerForName_removeAllDomainsSince() {
    yield setWithDate("a.com", "foo", 1, 100, null);
    yield setWithDate("b.com", "foo", 2, 200, null);
    yield setWithDate("c.com", "foo", 3, 300, null);

    yield setWithDate("a.com", "bar", 1, 0, null);
    yield setWithDate("b.com", "bar", 2, 100, null);
    yield setWithDate("c.com", "bar", 3, 200, null);
    yield setGlobal("foo", 2, null);

    yield cps.removeAllDomainsSince(200, null, makeCallback());

    let args = yield on("Removed", ["foo", "bar", null]);

    observerArgsOK(args.foo, [["b.com", "foo", false], ["c.com", "foo", false]]);
    observerArgsOK(args.bar, [["c.com", "bar", false]]);
    observerArgsOK(args.null, [["b.com", "foo", false], ["c.com", "bar", false], ["c.com", "foo", false]]);
  },

  function* observerForName_removeAllDomainsSince_private() {
    let context = privateLoadContext;
    yield setWithDate("a.com", "foo", 1, 100, context);
    yield setWithDate("b.com", "foo", 2, 200, context);
    yield setWithDate("c.com", "foo", 3, 300, context);

    yield setWithDate("a.com", "bar", 1, 0, context);
    yield setWithDate("b.com", "bar", 2, 100, context);
    yield setWithDate("c.com", "bar", 3, 200, context);
    yield setGlobal("foo", 2, context);

    yield cps.removeAllDomainsSince(200, context, makeCallback());

    let args = yield on("Removed", ["foo", "bar", null]);

    observerArgsOK(args.foo, [["a.com", "foo", true], ["b.com", "foo", true], ["c.com", "foo", true]]);
    observerArgsOK(args.bar, [["a.com", "bar", true], ["b.com", "bar", true], ["c.com", "bar", true]]);
    observerArgsOK(args.null, [["a.com", "foo", true], ["a.com", "bar", true],
                               ["b.com", "foo", true], ["b.com", "bar", true],
                               ["c.com", "foo", true], ["c.com", "bar", true]]);
  },
];
