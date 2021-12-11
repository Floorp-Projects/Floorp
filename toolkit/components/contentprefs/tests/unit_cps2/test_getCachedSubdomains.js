/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  getCachedSubdomainsOK(["a.com", "foo"], []);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  getCachedSubdomainsOK(["http://a.com/huh", "foo"], [["a.com", 1]]);
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);

  await set("a.com", "bar", 2);
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);

  await setGlobal("foo", 3);
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  getCachedGlobalOK(["foo"], true, 3);

  await setGlobal("bar", 4);
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  getCachedGlobalOK(["foo"], true, 3);
  getCachedGlobalOK(["bar"], true, 4);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", 1],
      ["b.a.com", 2],
    ]
  );
  getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);
  await reset();
});

add_task(async function populateViaGet() {
  await new Promise(resolve =>
    cps.getByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);

  await new Promise(resolve =>
    cps.getGlobal("foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);
  await reset();
});

add_task(async function populateViaGetSubdomains() {
  await new Promise(resolve =>
    cps.getBySubdomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  await reset();
});

add_task(async function populateViaRemove() {
  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);

  await new Promise(resolve =>
    cps.removeBySubdomainAndName("b.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);

  await new Promise(resolve =>
    cps.removeGlobal("foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);

  await set("a.com", "foo", 1);
  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);

  await set("a.com", "foo", 2);
  await set("b.a.com", "foo", 3);
  await new Promise(resolve =>
    cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", undefined],
      ["b.a.com", undefined],
    ]
  );
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);
  getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", undefined]]);

  await setGlobal("foo", 4);
  await new Promise(resolve =>
    cps.removeGlobal("foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", undefined],
      ["b.a.com", undefined],
    ]
  );
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);
  getCachedSubdomainsOK(["b.a.com", "foo"], [["b.a.com", undefined]]);
  await reset();
});

add_task(async function populateViaRemoveByDomain() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await set("b.a.com", "foo", 3);
  await set("b.a.com", "bar", 4);
  await new Promise(resolve =>
    cps.removeByDomain("a.com", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", undefined],
      ["b.a.com", 3],
    ]
  );
  getCachedSubdomainsOK(
    ["a.com", "bar"],
    [
      ["a.com", undefined],
      ["b.a.com", 4],
    ]
  );

  await set("a.com", "foo", 5);
  await set("a.com", "bar", 6);
  await new Promise(resolve =>
    cps.removeBySubdomain("a.com", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", undefined],
      ["b.a.com", undefined],
    ]
  );
  getCachedSubdomainsOK(
    ["a.com", "bar"],
    [
      ["a.com", undefined],
      ["b.a.com", undefined],
    ]
  );

  await setGlobal("foo", 7);
  await setGlobal("bar", 8);
  await new Promise(resolve =>
    cps.removeAllGlobals(null, makeCallback(resolve))
  );
  getCachedGlobalOK(["foo"], true, undefined);
  getCachedGlobalOK(["bar"], true, undefined);
  await reset();
});

add_task(async function populateViaRemoveAllDomains() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await set("b.com", "foo", 3);
  await set("b.com", "bar", 4);
  await new Promise(resolve =>
    cps.removeAllDomains(null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["b.com", "foo"], [["b.com", undefined]]);
  getCachedSubdomainsOK(["b.com", "bar"], [["b.com", undefined]]);
  await reset();
});

add_task(async function populateViaRemoveByName() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await new Promise(resolve =>
    cps.removeByName("foo", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  getCachedGlobalOK(["foo"], true, undefined);
  getCachedGlobalOK(["bar"], true, 4);

  await new Promise(resolve =>
    cps.removeByName("bar", null, makeCallback(resolve))
  );
  getCachedSubdomainsOK(["a.com", "foo"], [["a.com", undefined]]);
  getCachedSubdomainsOK(["a.com", "bar"], [["a.com", undefined]]);
  getCachedGlobalOK(["foo"], true, undefined);
  getCachedGlobalOK(["bar"], true, undefined);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);

  let context = privateLoadContext;
  await set("a.com", "foo", 6, context);
  await setGlobal("foo", 7, context);
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
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.getCachedBySubdomainAndName(null, "foo", null));
  do_check_throws(() => cps.getCachedBySubdomainAndName("", "foo", null));
  do_check_throws(() => cps.getCachedBySubdomainAndName("a.com", "", null));
  do_check_throws(() => cps.getCachedBySubdomainAndName("a.com", null, null));
  await reset();
});
