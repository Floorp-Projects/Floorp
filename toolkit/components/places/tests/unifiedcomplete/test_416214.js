/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test autocomplete for non-English URLs that match the tag bug 416214. Also
 * test bug 417441 by making sure escaped ascii characters like "+" remain
 * escaped.
 *
 * - add a visit for a page with a non-English URL
 * - add a tag for the page
 * - search for the tag
 * - test number of matches (should be exactly one)
 * - make sure the url is decoded
 */

add_task(async function test_tag_match_url() {
  info(
    "Make sure tag matches return the right url as well as '+' remain escaped"
  );
  let uri1 = NetUtil.newURI("http://escaped/ユニコード");
  let uri2 = NetUtil.newURI("http://asciiescaped/blocking-firefox3%2B");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "title",
    tags: ["superTag"],
    style: ["bookmark-tag"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "title",
    tags: ["superTag"],
    style: ["bookmark-tag"],
  });
  await check_autocomplete({
    search: "superTag",
    matches: [
      {
        uri: uri1,
        title: "title",
        tags: ["superTag"],
        style: ["bookmark-tag"],
      },
      {
        uri: uri2,
        title: "title",
        tags: ["superTag"],
        style: ["bookmark-tag"],
      },
    ],
  });
  await cleanup();
});
