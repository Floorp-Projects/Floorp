/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424717 to make sure searching with an existing location like
 * http://site/ also matches https://site/ or ftp://site/. Same thing for
 * ftp://site/ and https://site/.
 *
 * Test bug 461483 to make sure a search for "w" doesn't match the "www." from
 * site subdomains.
 */

add_task(async function test_swap_protocol() {
  let uri1 = NetUtil.newURI("http://www.site/");
  let uri2 = NetUtil.newURI("http://site/");
  let uri3 = NetUtil.newURI("ftp://ftp.site/");
  let uri4 = NetUtil.newURI("ftp://site/");
  let uri5 = NetUtil.newURI("https://www.site/");
  let uri6 = NetUtil.newURI("https://site/");
  let uri7 = NetUtil.newURI("http://woohoo/");
  let uri8 = NetUtil.newURI("http://wwwwwwacko/");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
    { uri: uri3, title: "title" },
    { uri: uri4, title: "title" },
    { uri: uri5, title: "title" },
    { uri: uri6, title: "title" },
    { uri: uri7, title: "title" },
    { uri: uri8, title: "title" },
  ]);

  // uri1 and uri2 won't appear since they are lower-ranked duplicates of uri6.
  let allMatches = [
    { uri: uri3, title: "title" },
    { uri: uri4, title: "title" },
    { uri: uri5, title: "title" },
    { uri: uri6, title: "title" },
  ];

  // Disable autoFill to avoid handling the first result.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);

  info("http://www.site matches 'www.site' pages");
  await check_autocomplete({
    search: "http://www.site",
    matches: [{ uri: uri5, title: "title" }],
  });

  info("http://site matches all site");
  await check_autocomplete({
    search: "http://site",
    matches: allMatches,
  });

  info("ftp://ftp.site matches itself");
  await check_autocomplete({
    search: "ftp://ftp.site",
    matches: [{ uri: uri3, title: "title" }],
  });

  info("ftp://site matches all site");
  await check_autocomplete({
    search: "ftp://site",
    matches: allMatches,
  });

  info("https://www.site matches all site");
  await check_autocomplete({
    search: "https://www.site",
    matches: [{ uri: uri5, title: "title" }],
  });

  info("https://site matches all site");
  await check_autocomplete({
    search: "https://site",
    matches: allMatches,
  });

  info("www.site matches 'www.site' pages");
  await check_autocomplete({
    search: "www.site",
    matches: [{ uri: uri5, title: "title" }],
  });

  info("w matches 'w' pages, including 'www'");
  await check_autocomplete({
    search: "w",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri7, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://w matches 'w' pages, including 'www'");
  await check_autocomplete({
    search: "http://w",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri7, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://www.w matches nothing");
  await check_autocomplete({
    search: "http://www.w",
    matches: [],
  });

  info("ww matches no 'ww' pages, including 'www'");
  await check_autocomplete({
    search: "ww",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://ww matches no 'ww' pages, including 'www'");
  await check_autocomplete({
    search: "http://ww",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://www.ww matches nothing");
  await check_autocomplete({
    search: "http://www.ww",
    matches: [],
  });

  info("www matches 'www' pages");
  await check_autocomplete({
    search: "www",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://www matches 'www' pages");
  await check_autocomplete({
    search: "http://www",
    matches: [
      { uri: uri5, title: "title" },
      { uri: uri8, title: "title" },
    ],
  });

  info("http://www.www matches nothing");
  await check_autocomplete({
    search: "http://www.www",
    matches: [],
  });

  await cleanup();
});
