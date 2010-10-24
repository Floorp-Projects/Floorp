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
 * The Original Code is Places Tagging Service unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com> (Original Author)
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

// Notice we use createInstance because later we will have to terminate the
// service and restart it.
var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
              createInstance().QueryInterface(Ci.nsITaggingService);

function run_test() {
  var options = PlacesUtils.history.getNewQueryOptions();
  var query = PlacesUtils.history.getNewQuery();

  query.setFolders([PlacesUtils.tagsFolderId], 1);
  var result = PlacesUtils.history.executeQuery(query, options);
  var tagRoot = result.root;
  tagRoot.containerOpen = true;

  do_check_eq(tagRoot.childCount, 0);

  var uri1 = uri("http://foo.tld/");
  var uri2 = uri("https://bar.tld/");

  // this also tests that the multiple folders are not created for the same tag
  tagssvc.tagURI(uri1, ["tag 1"]);
  tagssvc.tagURI(uri2, ["tag 1"]);
  do_check_eq(tagRoot.childCount, 1);

  var tag1node = tagRoot.getChild(0)
                        .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  var tag1itemId = tag1node.itemId;

  do_check_eq(tag1node.title, "tag 1");
  tag1node.containerOpen = true;
  do_check_eq(tag1node.childCount, 2);

  // Tagging the same url twice (or even thrice!) with the same tag should be a
  // no-op
  tagssvc.tagURI(uri1, ["tag 1"]);
  do_check_eq(tag1node.childCount, 2);
  tagssvc.tagURI(uri1, [tag1itemId]);
  do_check_eq(tag1node.childCount, 2);
  do_check_eq(tagRoot.childCount, 1);

  // also tests bug 407575
  tagssvc.tagURI(uri1, [tag1itemId, "tag 1", "tag 2", "Tag 1", "Tag 2"]);
  do_check_eq(tagRoot.childCount, 2);
  do_check_eq(tag1node.childCount, 2);

  // test getTagsForURI
  var uri1tags = tagssvc.getTagsForURI(uri1);
  do_check_eq(uri1tags.length, 2);
  do_check_eq(uri1tags[0], "Tag 1");
  do_check_eq(uri1tags[1], "Tag 2");
  var uri2tags = tagssvc.getTagsForURI(uri2);
  do_check_eq(uri2tags.length, 1);
  do_check_eq(uri2tags[0], "Tag 1");

  // test getURIsForTag
  var tag1uris = tagssvc.getURIsForTag("tag 1");
  do_check_eq(tag1uris.length, 2);
  do_check_true(tag1uris[0].equals(uri1));
  do_check_true(tag1uris[1].equals(uri2));

  // test allTags attribute
  var allTags = tagssvc.allTags;
  do_check_eq(allTags.length, 2);
  do_check_eq(allTags[0], "Tag 1");
  do_check_eq(allTags[1], "Tag 2");

  // test untagging
  tagssvc.untagURI(uri1, ["tag 1"]);
  do_check_eq(tag1node.childCount, 1);

  // removing the last uri from a tag should remove the tag-container
  tagssvc.untagURI(uri2, ["tag 1"]);
  do_check_eq(tagRoot.childCount, 1);

  // cleanup
  tag1node.containerOpen = false;

  // get array of tag folder ids => title
  // for testing tagging with mixed folder ids and tags
  var tagFolders = [];
  var child = tagRoot.getChild(0);
  var tagId = child.itemId;
  var tagTitle = child.title;

  // test mixed id/name tagging
  // as well as non-id numeric tags
  var uri3 = uri("http://testuri/3");
  tagssvc.tagURI(uri3, [tagId, "tag 3", "456"]);
  var tags = tagssvc.getTagsForURI(uri3);
  do_check_true(tags.indexOf(tagTitle) != -1);
  do_check_true(tags.indexOf("tag 3") != -1);
  do_check_true(tags.indexOf("456") != -1);

  // test mixed id/name tagging
  tagssvc.untagURI(uri3, [tagId, "tag 3", "456"]);
  tags = tagssvc.getTagsForURI(uri3);
  do_check_eq(tags.length, 0);

  // Terminate tagging service, fire up a new instance and check that existing
  // tags are there.  This will ensure that any internal caching system is
  // correctly filled at startup and we are not losing previously existing tags.
  var uri4 = uri("http://testuri/4");
  tagssvc.tagURI(uri4, [tagId, "tag 3", "456"]);
  tagssvc = null;
  tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
            getService(Ci.nsITaggingService);
  var uri4Tags = tagssvc.getTagsForURI(uri4);
  do_check_eq(uri4Tags.length, 3);
  do_check_true(uri4Tags.indexOf(tagTitle) != -1);
  do_check_true(uri4Tags.indexOf("tag 3") != -1);
  do_check_true(uri4Tags.indexOf("456") != -1);

  // Test sparse arrays.
  let (curChildCount = tagRoot.childCount) {
    try {
      tagssvc.tagURI(uri1, [, "tagSparse"]);
      do_check_eq(tagRoot.childCount, curChildCount + 1);
    } catch (ex) {
      do_throw("Passing a sparse array should not throw");
    }
    try {
      tagssvc.untagURI(uri1, [, "tagSparse"]);
      do_check_eq(tagRoot.childCount, curChildCount);
    } catch (ex) {
      do_throw("Passing a sparse array should not throw");
    }

    // Test that the API throws for bad arguments.
    try {
      tagssvc.tagURI(uri1, ["", "test"]);
      do_throw("Passing a bad tags array should throw");
    } catch (ex) {
      do_check_eq(ex.name, "NS_ERROR_ILLEGAL_VALUE");
    }
    try {
      tagssvc.untagURI(uri1, ["", "test"]);
      do_throw("Passing a bad tags array should throw");
    } catch (ex) {
      do_check_eq(ex.name, "NS_ERROR_ILLEGAL_VALUE");
    }
    try {
      tagssvc.tagURI(uri1, [0, "test"]);
      do_throw("Passing a bad tags array should throw");
    } catch (ex) {
      do_check_eq(ex.name, "NS_ERROR_ILLEGAL_VALUE");
    }
    try {
      tagssvc.tagURI(uri1, [0, "test"]);
      do_throw("Passing a bad tags array should throw");
    } catch (ex) {
      do_check_eq(ex.name, "NS_ERROR_ILLEGAL_VALUE");
    }
  }

  // cleanup
  tagRoot.containerOpen = false;
}
