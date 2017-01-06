/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  var child = tagRoot.getChild(0);
  var tagId = child.itemId;
  var tagTitle = child.title;

  // test mixed id/name tagging
  // as well as non-id numeric tags
  var uri3 = uri("http://testuri/3");
  tagssvc.tagURI(uri3, [tagId, "tag 3", "456"]);
  var tags = tagssvc.getTagsForURI(uri3);
  do_check_true(tags.includes(tagTitle));
  do_check_true(tags.includes("tag 3"));
  do_check_true(tags.includes("456"));

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
  do_check_true(uri4Tags.includes(tagTitle));
  do_check_true(uri4Tags.includes("tag 3"));
  do_check_true(uri4Tags.includes("456"));

  // Test sparse arrays.
  let curChildCount = tagRoot.childCount;

  try {
    tagssvc.tagURI(uri1, [undefined, "tagSparse"]);
    do_check_eq(tagRoot.childCount, curChildCount + 1);
  } catch (ex) {
    do_throw("Passing a sparse array should not throw");
  }
  try {
    tagssvc.untagURI(uri1, [undefined, "tagSparse"]);
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

  // Tag name length should be limited to nsITaggingService.MAX_TAG_LENGTH (bug407821)
  try {

    // generate a long tag name. i.e. looooo...oong_tag
    var n = Ci.nsITaggingService.MAX_TAG_LENGTH;
    var someOos = new Array(n).join('o');
    var longTagName = "l" + someOos + "ng_tag";

    tagssvc.tagURI(uri1, ["short_tag", longTagName]);
    do_throw("Passing a bad tags array should throw");

  } catch (ex) {
    do_check_eq(ex.name, "NS_ERROR_ILLEGAL_VALUE");
  }

  // cleanup
  tagRoot.containerOpen = false;

  // Tagging service should trim tags (Bug967196)
  let exampleURI = uri("http://www.example.com/");
  PlacesUtils.tagging.tagURI(exampleURI, [ " test " ]);

  let exampleTags = PlacesUtils.tagging.getTagsForURI(exampleURI);
  do_check_eq(exampleTags.length, 1);
  do_check_eq(exampleTags[0], "test");

  PlacesUtils.tagging.untagURI(exampleURI, [ "test" ]);
  exampleTags = PlacesUtils.tagging.getTagsForURI(exampleURI);
  do_check_eq(exampleTags.length, 0);
}
