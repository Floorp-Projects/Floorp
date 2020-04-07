/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 395161 that allows special searches that restrict results to
 * history/bookmark/tagged items and title/url matches.
 *
 * Test 485122 by making sure results don't have tags when restricting result
 * to just history either by default behavior or dynamic query restrict.
 */

function setSuggestPrefsToFalse() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
}

add_task(async function test_special_searches() {
  let uri1 = NetUtil.newURI("http://url/");
  let uri2 = NetUtil.newURI("http://url/2");
  let uri3 = NetUtil.newURI("http://foo.bar/");
  let uri4 = NetUtil.newURI("http://foo.bar/2");
  let uri5 = NetUtil.newURI("http://url/star");
  let uri6 = NetUtil.newURI("http://url/star/2");
  let uri7 = NetUtil.newURI("http://foo.bar/star");
  let uri8 = NetUtil.newURI("http://foo.bar/star/2");
  let uri9 = NetUtil.newURI("http://url/tag");
  let uri10 = NetUtil.newURI("http://url/tag/2");
  let uri11 = NetUtil.newURI("http://foo.bar/tag");
  let uri12 = NetUtil.newURI("http://foo.bar/tag/2");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title", transition: TRANSITION_TYPED },
    { uri: uri2, title: "foo.bar" },
    { uri: uri3, title: "title" },
    { uri: uri4, title: "foo.bar", transition: TRANSITION_TYPED },
    { uri: uri6, title: "foo.bar" },
    { uri: uri11, title: "title", transition: TRANSITION_TYPED },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri5, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri6, title: "foo.bar" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri7, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri8, title: "foo.bar" });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri9,
    title: "title",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri10,
    title: "foo.bar",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri11,
    title: "title",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri12,
    title: "foo.bar",
    tags: ["foo.bar"],
  });

  // Test restricting searches.

  info("History restrict");
  await check_autocomplete({
    search: UrlbarTokenizer.RESTRICT.HISTORY,
    matches: [
      { uri: uri1, title: "title" },
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info("Star restrict");
  await check_autocomplete({
    search: UrlbarTokenizer.RESTRICT.BOOKMARK,
    matches: [
      { uri: uri5, title: "title", style: ["bookmark"] },
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info("Tag restrict");
  await check_autocomplete({
    search: UrlbarTokenizer.RESTRICT.TAG,
    matches: [
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info("Special as first word");
  await check_autocomplete({
    search: `${UrlbarTokenizer.RESTRICT.HISTORY} foo bar`,
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info("Special as last word");
  await check_autocomplete({
    search: `foo bar ${UrlbarTokenizer.RESTRICT.HISTORY}`,
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  // Test restricting and matching searches with a term.

  info(`foo ${UrlbarTokenizer.RESTRICT.HISTORY} -> history`);
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.HISTORY}`,
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} -> is star`);
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
    matches: [
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.TITLE} -> in title`);
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.TITLE}`,
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["tag"] },
      { uri: uri10, title: "foo.bar", tags: ["foo.bar"], style: ["tag"] },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
      { uri: uri12, title: "foo.bar", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.URL} -> in url`);
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.URL}`,
    matches: [
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
      { uri: uri12, title: "foo.bar", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.TAG} -> is tag`);
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.TAG}`,
    matches: [
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  // Test various pairs of special searches

  info(
    `foo ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.TITLE} -> history, in title`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.TITLE}`,
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info(
    `foo ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.URL} -> history, in url`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.URL}`,
    matches: [
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info(
    `foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TITLE} -> is star, in title`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TITLE}`,
    matches: [
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info(
    `foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.URL} -> is star, in url`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.URL}`,
    matches: [
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info(
    `foo ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.TAG} -> in title, is tag`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.TAG}`,
    matches: [
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info(
    `foo ${UrlbarTokenizer.RESTRICT.URL} ${UrlbarTokenizer.RESTRICT.TAG} -> in url, is tag`
  );
  await check_autocomplete({
    search: `foo ${UrlbarTokenizer.RESTRICT.URL} ${UrlbarTokenizer.RESTRICT.TAG}`,
    matches: [
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  // Test conflicting restrictions.

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.URL} -> url wins`
  );
  await PlacesTestUtils.addVisits([
    {
      uri: `http://conflict.com/${UrlbarTokenizer.RESTRICT.TITLE}`,
      title: "test",
    },
    {
      uri: "http://conflict.com/",
      title: `test${UrlbarTokenizer.RESTRICT.TITLE}`,
    },
  ]);
  await check_autocomplete({
    search: `conflict ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.URL}`,
    matches: [
      {
        uri: `http://conflict.com/${UrlbarTokenizer.RESTRICT.TITLE}`,
        title: "test",
      },
    ],
  });

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.BOOKMARK} -> bookmark wins`
  );
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://bookmark.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.HISTORY}`,
  });
  await check_autocomplete({
    search: `conflict ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
    matches: [
      {
        uri: "http://bookmark.conflict.com/",
        title: `conflict ${UrlbarTokenizer.RESTRICT.HISTORY}`,
        style: ["bookmark"],
      },
    ],
  });

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TAG} -> tag wins`
  );
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
    tags: ["one"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://nontag.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
  });
  await check_autocomplete({
    search: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TAG}`,
    matches: [
      {
        uri: "http://tag.conflict.com/",
        title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
        tags: ["one"],
        style: ["bookmark-tag"],
      },
    ],
  });

  // Disable autoFill for the next tests, see test_autoFill_default_behavior.js
  // for specific tests.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  // Test default usage by setting certain browser.urlbar.suggest.* prefs
  info("foo -> default history");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  await check_autocomplete({
    search: "foo",
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar" },
      { uri: uri11, title: "title", tags: ["foo.bar"], style: ["tag"] },
    ],
  });

  info("foo -> default history, is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [
      { uri: uri2, title: "foo.bar" },
      { uri: uri3, title: "title" },
      { uri: uri4, title: "foo.bar" },
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  info("foo -> is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [
      { uri: uri6, title: "foo.bar", style: ["bookmark"] },
      { uri: uri7, title: "title", style: ["bookmark"] },
      { uri: uri8, title: "foo.bar", style: ["bookmark"] },
      { uri: uri9, title: "title", tags: ["foo.bar"], style: ["bookmark-tag"] },
      {
        uri: uri10,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri11,
        title: "title",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri12,
        title: "foo.bar",
        tags: ["foo.bar"],
        style: ["bookmark-tag"],
      },
    ],
  });

  await cleanup();
});
