/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  await setGlobal("foo", 1);
  await new Promise(resolve =>
    cps.removeAllDomains(null, makeCallback(resolve))
  );
  await dbOK([[null, "foo", 1]]);
  await getGlobalOK(["foo"], 1);
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
    cps.removeAllDomains(null, makeCallback(resolve))
  );
  await dbOK([
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], undefined);
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
  await new Promise(resolve =>
    cps.removeAllDomains(context, makeCallback(resolve))
  );
  await dbOK([
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo", context], undefined);
  await getOK(["a.com", "bar", context], undefined);
  await getGlobalOK(["foo", context], 7);
  await getGlobalOK(["bar", context], 4);
  await getOK(["b.com", "foo", context], undefined);

  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeAllDomains(null, "bogus"));
  await reset();
});

add_task(async function invalidateCache() {
  await set("a.com", "foo", 1);
  await set("b.com", "bar", 2);
  await setGlobal("baz", 3);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["b.com", "bar"], true, 2);
  getCachedGlobalOK(["baz"], true, 3);
  let promiseRemoved = new Promise(resolve =>
    cps.removeAllDomains(null, makeCallback(resolve))
  );
  getCachedOK(["a.com", "foo"], false);
  getCachedOK(["b.com", "bar"], false);
  getCachedGlobalOK(["baz"], true, 3);
  await promiseRemoved;
  await reset();
});
