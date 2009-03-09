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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

 /**
  * Check for correct functionality of PlacesUtils.setAnnotationsForItem/URI
  */

Components.utils.import("resource://gre/modules/utils.js");

var hs = PlacesUtils.history;
var bs = PlacesUtils.bookmarks;
var as = PlacesUtils.annotations;

const TEST_URL = "http://test.mozilla.org/";

function run_test() {
  var testURI = uri(TEST_URL);
  // add a bookmark
  var itemId = bs.insertBookmark(bs.unfiledBookmarksFolder, testURI,
                                 bs.DEFAULT_INDEX, "test");

  // create annotations array
  var testAnnos = [{ name: "testAnno/test0",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test0",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test1",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test1",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test2",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test2",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];

  // Add item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    do_check_true(as.itemHasAnnotation(itemId, anno.name));
    do_check_eq(as.getItemAnnotation(itemId, anno.name), anno.value);
  });

  // Add page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    do_check_true(as.pageHasAnnotation(testURI, anno.name));
    do_check_eq(as.getPageAnnotation(testURI, anno.name), anno.value);
  });

  // To unset annotations we set their values to null
  testAnnos.forEach(function(anno) { anno.value = null; });

  // Unset all item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    do_check_false(as.itemHasAnnotation(itemId, anno.name));
    // sanity: page annotations should not be removed here
    do_check_true(as.pageHasAnnotation(testURI, anno.name));
  });

  // Unset all page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    do_check_false(as.pageHasAnnotation(testURI, anno.name));
  });
}
