/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests the deprecated livemarks interface.

function run_test()
{
  let livemarkId = PlacesUtils.livemarks.createLivemarkFolderOnly(
    PlacesUtils.bookmarksMenuFolderId, "foo", uri("http://example.com/"),
    uri("http://example.com/rss.xml"), PlacesUtils.bookmarks.DEFAULT_INDEX
  );

  do_check_true(PlacesUtils.livemarks.isLivemark(livemarkId));
  do_check_eq(PlacesUtils.livemarks.getSiteURI(livemarkId).spec, "http://example.com/");
  do_check_eq(PlacesUtils.livemarks.getFeedURI(livemarkId).spec, "http://example.com/rss.xml");
  do_check_true(PlacesUtils.bookmarks.getFolderReadonly(livemarkId));

  // Make sure we can't add a livemark to a livemark
  let livemarkId2 = null;
  try {
    let livemarkId2 = PlacesUtils.livemarks.createLivemark(
      livemarkId, "foo", uri("http://example.com/"),
      uri("http://example.com/rss.xml"), PlacesUtils.bookmarks.DEFAULT_INDEX
    );
  } catch (ex) {
    livemarkId2 = null;
  }
  do_check_true(livemarkId2 == null);
  
  // make sure it didn't screw up the first one
  do_check_true(PlacesUtils.livemarks.isLivemark(livemarkId));

  do_check_eq(
    PlacesUtils.livemarks.getLivemarkIdForFeedURI(uri("http://example.com/rss.xml")),
    livemarkId
  );

  // make sure folders don't get counted as bookmarks
  // create folder
  let randomFolder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarksMenuFolderId, "Random",
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  do_check_true(!PlacesUtils.livemarks.isLivemark(randomFolder));
}
