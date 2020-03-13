/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  await set("a.com", "foo", 1);
  await setGlobal("foo", 2);

  await new Promise(resolve =>
    cps.removeByDomain("bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);

  await new Promise(resolve =>
    cps.removeBySubdomain("bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  await new Promise(resolve =>
    cps.removeByDomain("a.com", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);

  await set("a.com", "foo", 2);
  await new Promise(resolve =>
    cps.removeByDomain("http://a.com/huh", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);
  await reset();
});

add_task(async function domains() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  await new Promise(resolve =>
    cps.removeByDomain("a.com", null, makeCallback(resolve))
  );
  await dbOK([
    [null, "foo", 3],
    [null, "bar", 4],
    ["b.com", "foo", 5],
    ["b.com", "bar", 6],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], 5);
  await getOK(["b.com", "bar"], 6);

  await new Promise(resolve =>
    cps.removeAllGlobals(null, makeCallback(resolve))
  );
  await dbOK([
    ["b.com", "foo", 5],
    ["b.com", "bar", 6],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], undefined);
  await getOK(["b.com", "foo"], 5);
  await getOK(["b.com", "bar"], 6);

  await new Promise(resolve =>
    cps.removeByDomain("b.com", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], undefined);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], undefined);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  await new Promise(resolve =>
    cps.removeByDomain("a.com", null, makeCallback(resolve))
  );
  await dbOK([["b.a.com", "foo", 2]]);
  await getSubdomainsOK(["a.com", "foo"], [["b.a.com", 2]]);
  await getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);

  await set("a.com", "foo", 3);
  await new Promise(resolve =>
    cps.removeBySubdomain("a.com", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getSubdomainsOK(["a.com", "foo"], []);
  await getSubdomainsOK(["b.a.com", "foo"], []);

  await set("a.com", "foo", 4);
  await set("b.a.com", "foo", 5);
  await new Promise(resolve =>
    cps.removeByDomain("b.a.com", null, makeCallback(resolve))
  );
  await dbOK([["a.com", "foo", 4]]);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
  await getSubdomainsOK(["b.a.com", "foo"], []);

  await set("b.a.com", "foo", 6);
  await new Promise(resolve =>
    cps.removeBySubdomain("b.a.com", null, makeCallback(resolve))
  );
  await dbOK([["a.com", "foo", 4]]);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
  await getSubdomainsOK(["b.a.com", "foo"], []);
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
  await set("b.com", "foo", 7, context);
  await setGlobal("foo", 8, context);
  await new Promise(resolve =>
    cps.removeByDomain("a.com", context, makeCallback(resolve))
  );
  await getOK(["b.com", "foo", context], 7);
  await getGlobalOK(["foo", context], 8);
  await new Promise(resolve =>
    cps.removeAllGlobals(context, makeCallback(resolve))
  );
  await dbOK([["b.com", "foo", 5]]);
  await getOK(["a.com", "foo", context], undefined);
  await getOK(["a.com", "bar", context], undefined);
  await getGlobalOK(["foo", context], undefined);
  await getGlobalOK(["bar", context], undefined);
  await getOK(["b.com", "foo", context], 5);

  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], undefined);
  await getOK(["b.com", "foo"], 5);
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeByDomain(null, null));
  do_check_throws(() => cps.removeByDomain("", null));
  do_check_throws(() => cps.removeByDomain("a.com", null, "bogus"));
  do_check_throws(() => cps.removeBySubdomain(null, null));
  do_check_throws(() => cps.removeBySubdomain("", null));
  do_check_throws(() => cps.removeBySubdomain("a.com", null, "bogus"));
  do_check_throws(() => cps.removeAllGlobals(null, "bogus"));
  await reset();
});

add_task(async function removeByDomain_invalidateCache() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["a.com", "bar"], true, 2);
  let promiseRemoved = new Promise(resolve => {
    cps.removeByDomain("a.com", null, makeCallback(resolve));
  });
  getCachedOK(["a.com", "foo"], false);
  getCachedOK(["a.com", "bar"], false);
  await promiseRemoved;
  await reset();
});

add_task(async function removeBySubdomain_invalidateCache() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", 1],
      ["b.a.com", 2],
    ]
  );
  let promiseRemoved = new Promise(resolve => {
    cps.removeBySubdomain("a.com", null, makeCallback(resolve));
  });
  getCachedSubdomainsOK(["a.com", "foo"], []);
  await promiseRemoved;
  await reset();
});

add_task(async function removeAllGlobals_invalidateCache() {
  await setGlobal("foo", 1);
  await setGlobal("bar", 2);
  getCachedGlobalOK(["foo"], true, 1);
  getCachedGlobalOK(["bar"], true, 2);
  let promiseRemoved = new Promise(resolve => {
    cps.removeAllGlobals(null, makeCallback(resolve));
  });
  getCachedGlobalOK(["foo"], false);
  getCachedGlobalOK(["bar"], false);
  await promiseRemoved;
  await reset();
});
