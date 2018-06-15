/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  // TODO: Improve testing coverage for QueryToQueryString and QueryStringToQuery

  // Bug 376798
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  query.setParents([PlacesUtils.bookmarks.rootGuid], 1);
  Assert.equal(PlacesUtils.history.queryToQueryString(query, options),
               `place:parent=${PlacesUtils.bookmarks.rootGuid}`);
}
