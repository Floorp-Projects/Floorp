/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test to make sure matches against the url, title, tags are first made on word
 * boundaries, instead of in the middle of words, and later are extended to the
 * whole words. For this test it is critical to check sorting of the matches.
 *
 * Make sure we don't try matching one after a CamelCase because the upper-case
 * isn't really a word boundary. (bug 429498)
 */

var katakana = ["\u30a8", "\u30c9"]; // E, Do
var ideograph = ["\u4efb", "\u5929", "\u5802"]; // Nin Ten Do

add_task(async function test_escape() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  await PlacesTestUtils.addVisits([
    { uri: "http://matchme/", title: "title1" },
    { uri: "http://dontmatchme/", title: "title1" },
    { uri: "http://title/1", title: "matchme2" },
    { uri: "http://title/2", title: "dontmatchme3" },
    { uri: "http://tag/1", title: "title1" },
    { uri: "http://tag/2", title: "title1" },
    { uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" },
    { uri: "http://katakana/", title: katakana.join("") },
    { uri: "http://ideograph/", title: ideograph.join("") },
    { uri: "http://camel/pleaseMatchMe/", title: "title1" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag/1",
    title: "title1",
    tags: ["matchme2"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag/2",
    title: "title1",
    tags: ["dontmatchme3"],
  });

  info("Match 'match' at the beginning or after / or on a CamelCase");
  await check_autocomplete({
    search: "match",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://camel/pleaseMatchMe/", title: "title1" },
      { uri: "http://title/1", title: "matchme2" },
      { uri: "http://matchme/", title: "title1" },
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://dontmatchme/", title: "title1" },
    ],
  });

  info("Match 'dont' at the beginning or after /");
  await check_autocomplete({
    search: "dont",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://dontmatchme/", title: "title1" },
    ],
  });

  info("Match 'match' at the beginning or after / or on a CamelCase");
  await check_autocomplete({
    search: "2",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://title/1", title: "matchme2" },
    ],
  });

  info("Match 't' at the beginning or after /");
  await check_autocomplete({
    search: "t",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://camel/pleaseMatchMe/", title: "title1" },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://title/1", title: "matchme2" },
      { uri: "http://dontmatchme/", title: "title1" },
      { uri: "http://matchme/", title: "title1" },
      { uri: "http://katakana/", title: katakana.join("") },
      { uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" },
    ],
  });

  info("Match 'word' after many consecutive word boundaries");
  await check_autocomplete({
    search: "word",
    checkSorting: true,
    matches: [{ uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" }],
  });

  info("Match a word boundary '/' for everything");
  await check_autocomplete({
    search: "/",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://camel/pleaseMatchMe/", title: "title1" },
      { uri: "http://ideograph/", title: ideograph.join("") },
      { uri: "http://katakana/", title: katakana.join("") },
      { uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://title/1", title: "matchme2" },
      { uri: "http://dontmatchme/", title: "title1" },
      { uri: "http://matchme/", title: "title1" },
    ],
  });

  info("Match word boundaries '()_' that are among word boundaries");
  await check_autocomplete({
    search: "()_",
    checkSorting: true,
    matches: [{ uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" }],
  });

  info("Katakana characters form a string, so match the beginning");
  await check_autocomplete({
    search: katakana[0],
    checkSorting: true,
    matches: [{ uri: "http://katakana/", title: katakana.join("") }],
  });

  /*
  info("Middle of a katakana word shouldn't be matched");
  await check_autocomplete({
    search: katakana[1],
    matches: [ ],
  });
*/

  info("Ideographs are treated as words so 'nin' is one word");
  await check_autocomplete({
    search: ideograph[0],
    checkSorting: true,
    matches: [{ uri: "http://ideograph/", title: ideograph.join("") }],
  });

  info("Ideographs are treated as words so 'ten' is another word");
  await check_autocomplete({
    search: ideograph[1],
    checkSorting: true,
    matches: [{ uri: "http://ideograph/", title: ideograph.join("") }],
  });

  info("Ideographs are treated as words so 'do' is yet another word");
  await check_autocomplete({
    search: ideograph[2],
    checkSorting: true,
    matches: [{ uri: "http://ideograph/", title: ideograph.join("") }],
  });

  info("Match in the middle. Should just be sorted by frecency.");
  await check_autocomplete({
    search: "ch",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://camel/pleaseMatchMe/", title: "title1" },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://title/1", title: "matchme2" },
      { uri: "http://dontmatchme/", title: "title1" },
      { uri: "http://matchme/", title: "title1" },
    ],
  });

  // Also this test should just be sorted by frecency.
  info(
    "Don't match one character after a camel-case word boundary (bug 429498). Should just be sorted by frecency."
  );
  await check_autocomplete({
    search: "atch",
    checkSorting: true,
    matches: [
      {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
        style: ["bookmark-tag"],
      },
      {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
        style: ["bookmark-tag"],
      },
      { uri: "http://camel/pleaseMatchMe/", title: "title1" },
      { uri: "http://title/2", title: "dontmatchme3" },
      { uri: "http://title/1", title: "matchme2" },
      { uri: "http://dontmatchme/", title: "title1" },
      { uri: "http://matchme/", title: "title1" },
    ],
  });

  await cleanup();
});
