/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function get_nonexistent() {
  await getSubdomainsOK(["a.com", "foo"], []);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  await getSubdomainsOK(["http://a.com/huh", "foo"], [["a.com", 1]]);
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);

  await set("a.com", "bar", 2);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  await getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);

  await setGlobal("foo", 3);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  await getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1], ["b.a.com", 2]]);
  await getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);
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
  await getSubdomainsOK(["a.com", "foo", context], [["a.com", 6]]);
  await getSubdomainsOK(["a.com", "bar", context], [["a.com", 2]]);
  await getSubdomainsOK(["b.com", "foo", context], [["b.com", 5]]);

  await getSubdomainsOK(["a.com", "foo"], [["a.com", 1]]);
  await getSubdomainsOK(["a.com", "bar"], [["a.com", 2]]);
  await getSubdomainsOK(["b.com", "foo"], [["b.com", 5]]);
  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.getBySubdomainAndName(null, "foo", null, {}));
  do_check_throws(() => cps.getBySubdomainAndName("", "foo", null, {}));
  do_check_throws(() => cps.getBySubdomainAndName("a.com", "", null, {}));
  do_check_throws(() => cps.getBySubdomainAndName("a.com", null, null, {}));
  do_check_throws(() => cps.getBySubdomainAndName("a.com", "foo", null, null));
  await reset();
});
