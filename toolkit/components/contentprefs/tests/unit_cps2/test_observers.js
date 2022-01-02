/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let global = this;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(async function resetBeforeTests() {
  await reset();
});

function raceWithIdleDispatch(promise) {
  return Promise.race([
    promise,
    new Promise(resolve => {
      Services.tm.idleDispatchToMainThread(() => resolve(null));
    }),
  ]);
}

var tests = [
  async function observerForName_set(context) {
    let argsPromise;
    argsPromise = on("Set", ["foo", null, "bar"]);
    await set("a.com", "foo", 1, context);
    let args = await argsPromise;
    observerArgsOK(args.foo, [["a.com", "foo", 1, context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [
      ["a.com", "foo", 1, context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.bar, []);

    argsPromise = on("Set", ["foo", null, "bar"]);
    await setGlobal("foo", 2, context);
    args = await argsPromise;
    observerArgsOK(args.foo, [[null, "foo", 2, context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", 2, context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
    await reset();
  },

  async function observerForName_remove(context) {
    let argsPromise;
    await set("a.com", "foo", 1, context);
    await setGlobal("foo", 2, context);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeByDomainAndName(
        "a.com",
        "bogus",
        context,
        makeCallback(resolve)
      )
    );
    let args = await raceWithIdleDispatch(argsPromise);
    strictEqual(args, null);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeByDomainAndName("a.com", "foo", context, makeCallback(resolve))
    );
    args = await argsPromise;
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeGlobal("foo", context, makeCallback(resolve))
    );
    args = await argsPromise;
    observerArgsOK(args.foo, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
    await reset();
  },

  async function observerForName_removeByDomain(context) {
    let argsPromise;
    await set("a.com", "foo", 1, context);
    await set("b.a.com", "bar", 2, context);
    await setGlobal("foo", 3, context);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeByDomain("bogus", context, makeCallback(resolve))
    );
    let args = await raceWithIdleDispatch(argsPromise);
    strictEqual(args, null);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeBySubdomain("a.com", context, makeCallback(resolve))
    );
    args = await argsPromise;
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [
      ["a.com", "foo", context.usePrivateBrowsing],
      ["b.a.com", "bar", context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.bar, [["b.a.com", "bar", context.usePrivateBrowsing]]);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeAllGlobals(context, makeCallback(resolve))
    );
    args = await argsPromise;
    observerArgsOK(args.foo, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [[null, "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.bar, []);
    await reset();
  },

  async function observerForName_removeAllDomains(context) {
    let argsPromise;
    await set("a.com", "foo", 1, context);
    await setGlobal("foo", 2, context);
    await set("b.com", "bar", 3, context);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeAllDomains(context, makeCallback(resolve))
    );
    let args = await argsPromise;
    observerArgsOK(args.foo, [["a.com", "foo", context.usePrivateBrowsing]]);
    observerArgsOK(args.null, [
      ["a.com", "foo", context.usePrivateBrowsing],
      ["b.com", "bar", context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.bar, [["b.com", "bar", context.usePrivateBrowsing]]);
    await reset();
  },

  async function observerForName_removeByName(context) {
    let argsPromise;
    await set("a.com", "foo", 1, context);
    await set("a.com", "bar", 2, context);
    await setGlobal("foo", 3, context);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeByName("bogus", context, makeCallback(resolve))
    );
    let args = await raceWithIdleDispatch(argsPromise);
    strictEqual(args, null);

    argsPromise = on("Removed", ["foo", null, "bar"]);
    await new Promise(resolve =>
      cps.removeByName("foo", context, makeCallback(resolve))
    );
    args = await argsPromise;
    observerArgsOK(args.foo, [
      ["a.com", "foo", context.usePrivateBrowsing],
      [null, "foo", context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.null, [
      ["a.com", "foo", context.usePrivateBrowsing],
      [null, "foo", context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.bar, []);
    await reset();
  },

  async function removeObserverForName(context) {
    let { promise, observers } = onEx("Set", ["foo", null, "bar"]);
    cps.removeObserverForName("foo", observers.foo);
    await set("a.com", "foo", 1, context);
    await wait();
    let args = await promise;
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, [
      ["a.com", "foo", 1, context.usePrivateBrowsing],
    ]);
    observerArgsOK(args.bar, []);
    args.reset();

    cps.removeObserverForName(null, args.null.observer);
    await set("a.com", "foo", 2, context);
    await wait();
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();
    await reset();
  },
];

// These tests are for functionality that doesn't behave the same way in private and public
// contexts, so the expected results cannot be automatically generated like the previous tests.
var specialTests = [
  async function observerForName_removeAllDomainsSince() {
    let argsPromise;
    await setWithDate("a.com", "foo", 1, 100, null);
    await setWithDate("b.com", "foo", 2, 200, null);
    await setWithDate("c.com", "foo", 3, 300, null);

    await setWithDate("a.com", "bar", 1, 0, null);
    await setWithDate("b.com", "bar", 2, 100, null);
    await setWithDate("c.com", "bar", 3, 200, null);
    await setGlobal("foo", 2, null);

    argsPromise = on("Removed", ["foo", "bar", null]);
    await new Promise(resolve =>
      cps.removeAllDomainsSince(200, null, makeCallback(resolve))
    );

    let args = await argsPromise;

    observerArgsOK(args.foo, [
      ["b.com", "foo", false],
      ["c.com", "foo", false],
    ]);
    observerArgsOK(args.bar, [["c.com", "bar", false]]);
    observerArgsOK(args.null, [
      ["b.com", "foo", false],
      ["c.com", "bar", false],
      ["c.com", "foo", false],
    ]);
    await reset();
  },

  async function observerForName_removeAllDomainsSince_private() {
    let argsPromise;
    let context = privateLoadContext;
    await setWithDate("a.com", "foo", 1, 100, context);
    await setWithDate("b.com", "foo", 2, 200, context);
    await setWithDate("c.com", "foo", 3, 300, context);

    await setWithDate("a.com", "bar", 1, 0, context);
    await setWithDate("b.com", "bar", 2, 100, context);
    await setWithDate("c.com", "bar", 3, 200, context);
    await setGlobal("foo", 2, context);

    argsPromise = on("Removed", ["foo", "bar", null]);
    await new Promise(resolve =>
      cps.removeAllDomainsSince(200, context, makeCallback(resolve))
    );

    let args = await argsPromise;

    observerArgsOK(args.foo, [
      ["a.com", "foo", true],
      ["b.com", "foo", true],
      ["c.com", "foo", true],
    ]);
    observerArgsOK(args.bar, [
      ["a.com", "bar", true],
      ["b.com", "bar", true],
      ["c.com", "bar", true],
    ]);
    observerArgsOK(args.null, [
      ["a.com", "foo", true],
      ["a.com", "bar", true],
      ["b.com", "foo", true],
      ["b.com", "bar", true],
      ["c.com", "foo", true],
      ["c.com", "bar", true],
    ]);
    await reset();
  },
];

for (let i = 0; i < tests.length; i++) {
  // Generate two wrappers of each test function that invoke the original test with an
  // appropriate privacy context.
  /* eslint-disable no-eval */
  var pub = eval(
    "var f = async function " +
      tests[i].name +
      "() { await tests[" +
      i +
      "](privateLoadContext); }; f"
  );
  var priv = eval(
    "var f = async function " +
      tests[i].name +
      "_private() { await tests[" +
      i +
      "](privateLoadContext); }; f"
  );
  /* eslint-enable no-eval */
  add_task(pub);
  add_task(priv);
}

for (let test of specialTests) {
  add_task(test);
}
