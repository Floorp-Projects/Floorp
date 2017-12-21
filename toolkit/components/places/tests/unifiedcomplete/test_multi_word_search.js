/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 401869 to allow multiple words separated by spaces to match in
 * the page title, page url, or bookmark title to be considered a match. All
 * terms must match but not all terms need to be in the title, etc.
 *
 * Test bug 424216 by making sure bookmark titles are always shown if one is
 * available. Also bug 425056 makes sure matches aren't found partially in the
 * page title and partially in the bookmark.
 */

add_task(async function test_match_beginning() {
  let uri1 = NetUtil.newURI("http://a.b.c/d-e_f/h/t/p");
  let uri2 = NetUtil.newURI("http://d.e.f/g-h_i/h/t/p");
  let uri3 = NetUtil.newURI("http://g.h.i/j-k_l/h/t/p");
  let uri4 = NetUtil.newURI("http://j.k.l/m-n_o/h/t/p");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "f(o)o b<a>r" },
    { uri: uri2, title: "b(a)r b<a>z" },
    { uri: uri3, title: "f(o)o b<a>r" },
    { uri: uri4, title: "f(o)o b<a>r" }
  ]);
  await addBookmark({ uri: uri3, title: "f(o)o b<a>r" });
  await addBookmark({ uri: uri4, title: "b(a)r b<a>z" });

  info("Match 2 terms all in url");
  await check_autocomplete({
    search: "c d",
    matches: [ { uri: uri1, title: "f(o)o b<a>r" } ]
  });

  info("Match 1 term in url and 1 term in title");
  await check_autocomplete({
    search: "b e",
    matches: [ { uri: uri1, title: "f(o)o b<a>r" },
               { uri: uri2, title: "b(a)r b<a>z" } ]
  });

  info("Match 3 terms all in title; display bookmark title if matched");
  await check_autocomplete({
    search: "b a z",
    matches: [ { uri: uri2, title: "b(a)r b<a>z" },
               { uri: uri4, title: "b(a)r b<a>z", style: [ "bookmark" ] } ]
  });

  info("Match 2 terms in url and 1 in title; make sure bookmark title is used for search");
  await check_autocomplete({
    search: "k f t",
    matches: [ { uri: uri3, title: "f(o)o b<a>r", style: [ "bookmark" ] } ]
  });

  info("Match 3 terms in url and 1 in title");
  await check_autocomplete({
    search: "d i g z",
    matches: [ { uri: uri2, title: "b(a)r b<a>z" } ]
  });

  info("Match nothing");
  await check_autocomplete({
    search: "m o z i",
    matches: [ ]
  });

  await cleanup();
});
