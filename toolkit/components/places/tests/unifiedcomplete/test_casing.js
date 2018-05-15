/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_casing_1() {
  info("Searching for cased entry 1");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
  });
  await check_autocomplete({
    search: "MOZ",
    autofilled: "MOZilla.org/",
    completed: "http://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_casing_2() {
  info("Searching for cased entry 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
  });
  await check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/T",
    completed: "mozilla.org/T"
  });
  await cleanup();
});

add_task(async function test_casing_3() {
  info("Searching for cased entry 3");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_casing_4() {
  info("Searching for cased entry 4");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "mOzilla.org/t",
    autofilled: "mOzilla.org/t",
    completed: "mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_casing_5() {
  info("Searching for cased entry 5");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "mOzilla.org/T",
    autofilled: "mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_casing() {
  info("Searching for untrimmed cased entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://mOz",
    autofilled: "http://mOzilla.org/",
    completed: "http://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_www_casing() {
  info("Searching for untrimmed cased entry with www");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://www.mOz",
    autofilled: "http://www.mOzilla.org/",
    completed: "http://www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_casing() {
  info("Searching for untrimmed cased entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://mOzilla.org/t",
    autofilled: "http://mOzilla.org/t",
    completed: "http://mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_casing_2() {
  info("Searching for untrimmed cased entry with path 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://mOzilla.org/T",
    autofilled: "http://mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_www_casing() {
  info("Searching for untrimmed cased entry with www and path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://www.mOzilla.org/t",
    autofilled: "http://www.mOzilla.org/t",
    completed: "http://www.mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_www_casing_2() {
  info("Searching for untrimmed cased entry with www and path 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
  });
  await check_autocomplete({
    search: "http://www.mOzilla.org/T",
    autofilled: "http://www.mOzilla.org/Test/",
    completed: "http://www.mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_searching() {
  let uri1 = NetUtil.newURI("http://dummy/1/");
  let uri2 = NetUtil.newURI("http://dummy/2/");
  let uri3 = NetUtil.newURI("http://dummy/3/");
  let uri4 = NetUtil.newURI("http://dummy/4/");
  let uri5 = NetUtil.newURI("http://dummy/5/");

  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "uppercase lambda \u039B" },
    { uri: uri2, title: "lowercase lambda \u03BB" },
    { uri: uri3, title: "symbol \u212A" }, // kelvin
    { uri: uri4, title: "uppercase K" },
    { uri: uri5, title: "lowercase k" },
  ]);

  info("Search for lowercase lambda");
  await check_autocomplete({
    search: "\u03BB",
    matches: [ { uri: uri1, title: "uppercase lambda \u039B" },
               { uri: uri2, title: "lowercase lambda \u03BB" } ]
  });

  info("Search for uppercase lambda");
  await check_autocomplete({
    search: "\u039B",
    matches: [ { uri: uri1, title: "uppercase lambda \u039B" },
               { uri: uri2, title: "lowercase lambda \u03BB" } ]
  });

  info("Search for kelvin sign");
  await check_autocomplete({
    search: "\u212A",
    matches: [ { uri: uri3, title: "symbol \u212A" },
               { uri: uri4, title: "uppercase K" },
               { uri: uri5, title: "lowercase k" } ]
  });

  info("Search for lowercase k");
  await check_autocomplete({
    search: "k",
    matches: [ { uri: uri3, title: "symbol \u212A" },
               { uri: uri4, title: "uppercase K" },
               { uri: uri5, title: "lowercase k" } ]
  });

  info("Search for uppercase k");
  await check_autocomplete({
    search: "K",
    matches: [ { uri: uri3, title: "symbol \u212A" },
               { uri: uri4, title: "uppercase K" },
               { uri: uri5, title: "lowercase k" } ]
  });

  await cleanup();
});
