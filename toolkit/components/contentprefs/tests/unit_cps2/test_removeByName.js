/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  await set("a.com", "foo", 1);
  await setGlobal("foo", 2);

  await new Promise(resolve => cps.removeByName("bogus", null, makeCallback(resolve)));
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  await new Promise(resolve => cps.removeByName("foo", null, makeCallback(resolve)));
  await dbOK([
    ["a.com", "bar", 2],
    [null, "bar", 4],
    ["b.com", "bar", 6],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], 6);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  let context = privateLoadContext;
  await set("a.com", "foo", 7, context);
  await setGlobal("foo", 8, context);
  await set("b.com", "bar", 9, context);
  await new Promise(resolve => cps.removeByName("bar", context, makeCallback(resolve)));
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 3],
    ["b.com", "foo", 5],
  ]);
  await getOK(["a.com", "foo", context], 7);
  await getOK(["a.com", "bar", context], undefined);
  await getGlobalOK(["foo", context], 8);
  await getGlobalOK(["bar", context], undefined);
  await getOK(["b.com", "foo", context], 5);
  await getOK(["b.com", "bar", context], undefined);

  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], undefined);
  await getOK(["b.com", "foo"], 5);
  await getOK(["b.com", "bar"], undefined);
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeByName("", null));
  do_check_throws(() => cps.removeByName(null, null));
  do_check_throws(() => cps.removeByName("foo", null, "bogus"));
  await reset();
});

add_task(async function invalidateCache() {
  await set("a.com", "foo", 1);
  await set("b.com", "foo", 2);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["b.com", "foo"], true, 2);
  cps.removeByName("foo", null, makeCallback());
  getCachedOK(["a.com", "foo"], false);
  getCachedOK(["b.com", "foo"], false);
  await reset();
});
