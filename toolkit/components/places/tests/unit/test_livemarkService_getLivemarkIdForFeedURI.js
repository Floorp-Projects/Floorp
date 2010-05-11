/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const LMANNO_FEEDURI = "livemark/feedURI";

var PU = PlacesUtils;
var bs = PU.bookmarks;
var as = PU.annotations;

/**
 * Creates a fake livemark without using the livemark service.
 * 
 * @param aParentId
 *        folder where we will create the livemark.
 * @param aTitle
 *        title for the livemark.
 * @param aSiteURI
 *        siteURI, currently unused.
 * @param aFeedURI
 *        feed URI for the livemark.
 * @param aIndex
 *        index of the new folder inside parent.
 *
 * @returns the new folder id.
 */
function createFakeLivemark(aParentId, aTitle, aSiteURI, aFeedURI, aIndex) {
  var folderId = bs.createFolder(aParentId, aTitle, aIndex);
  bs.setFolderReadonly(folderId, true);

  // Add an annotation to map the folder id to the livemark feed URI.
  as.setItemAnnotation(folderId, LMANNO_FEEDURI, aFeedURI.spec,
                       0, as.EXPIRE_NEVER);
  return folderId;
}

// main
function run_test() {
  var feedURI = uri("http://example.com/rss.xml");

  // Create a fake livemark, and use getMostRecentFolderForFeedURI before
  // livemark service is initialized.
  var fakeLivemarkId = createFakeLivemark(bs.bookmarksMenuFolder, "foo",
                                          uri("http://example.com/"),
                                          feedURI, bs.DEFAULT_INDEX);
  do_check_eq(PU.getMostRecentFolderForFeedURI(feedURI), fakeLivemarkId);

  bs.removeItem(fakeLivemarkId);
  do_check_eq(PU.getMostRecentFolderForFeedURI(feedURI), -1);

  // Create livemark service and repeat the test.
  var ls = PU.livemarks;
  var livemarkId = ls.createLivemarkFolderOnly(bs.bookmarksMenuFolder, "foo",
                                               uri("http://example.com/"),
                                               feedURI, bs.DEFAULT_INDEX);

  do_check_eq(ls.getLivemarkIdForFeedURI(feedURI), livemarkId);
  do_check_eq(PU.getMostRecentFolderForFeedURI(feedURI), livemarkId);

  bs.removeItem(livemarkId);
  do_check_eq(ls.getLivemarkIdForFeedURI(feedURI), -1);
  do_check_eq(PU.getMostRecentFolderForFeedURI(feedURI), -1);
}
