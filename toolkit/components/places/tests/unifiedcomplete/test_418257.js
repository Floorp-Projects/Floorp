/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 418257 by making sure tags are returned with the title as part of
 * the "comment" if there are tags even if we didn't match in the tags. They
 * are separated from the title by a endash.
 */

add_task(function* test_javascript_match() {
  let uri1 = NetUtil.newURI("http://page1");
  let uri2 = NetUtil.newURI("http://page2");
  let uri3 = NetUtil.newURI("http://page3");
  let uri4 = NetUtil.newURI("http://page4");
  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "tagged" },
    { uri: uri2, title: "tagged" },
    { uri: uri3, title: "tagged" },
    { uri: uri4, title: "tagged" }
  ]);
  yield addBookmark({ uri: uri1,
                      title: "tagged",
                      tags: [ "tag1" ] });
  yield addBookmark({ uri: uri2,
                      title: "tagged",
                      tags: [ "tag1", "tag2" ] });
  yield addBookmark({ uri: uri3,
                      title: "tagged",
                      tags: [ "tag1", "tag3" ] });
  yield addBookmark({ uri: uri4,
                      title: "tagged",
                      tags: [ "tag1", "tag2", "tag3" ] });

  do_print("Make sure tags come back in the title when matching tags");
  yield check_autocomplete({
    search: "page1 tag",
    matches: [ { uri: uri1, title: "tagged", tags: [ "tag1" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("Check tags in title for page2");
  yield check_autocomplete({
    search: "page2 tag",
    matches: [ { uri: uri2, title: "tagged", tags: [ "tag1", "tag2" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("Make sure tags appear even when not matching the tag");
  yield check_autocomplete({
    search: "page3",
    matches: [ { uri: uri3, title: "tagged", tags: [ "tag1", "tag3" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("Multiple tags come in commas for page4");
  yield check_autocomplete({
    search: "page4",
    matches: [ { uri: uri4, title: "tagged", tags: [ "tag1", "tag2", "tag3" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("Extra test just to make sure we match the title");
  yield check_autocomplete({
    search: "tag2",
    matches: [ { uri: uri2, title: "tagged", tags: [ "tag1", "tag2" ], style: [ "bookmark-tag" ] },
               { uri: uri4, title: "tagged", tags: [ "tag1", "tag2", "tag3" ], style: [ "bookmark-tag" ] } ]
  });

  yield cleanup();
});
