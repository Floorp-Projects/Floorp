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
 * The Original Code is Places Dynamic Containers unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com> (Original Author)
 *   Marco Bonardo <mak77@bonardo.net>
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

 try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                getService(Ci.nsIAnnotationService);
} catch (ex) {
  do_throw("Could not get services\n");
}
// main
function run_test() {
  // load our dynamic-container sample service
  do_load_module("/toolkit/components/places/tests/unit/nsDynamicContainerServiceSample.js");
  var testRoot = bmsvc.createFolder(bmsvc.placesRoot, "test root", bmsvc.DEFAULT_INDEX);
  var exposedFolder = bmsvc.createFolder(testRoot, "exposed folder", bmsvc.DEFAULT_INDEX);
  var efId1 = bmsvc.insertBookmark(exposedFolder, uri("http://uri1.tld"), bmsvc.DEFAULT_INDEX, "");

  // create our dynamic container
  var remoteContainer =
    bmsvc.createDynamicContainer(testRoot, "remote container sample",
                                "@mozilla.org/browser/remote-container-sample;1",
                                bmsvc.DEFAULT_INDEX);

  // the service will read this annotation and append a folder node for
  // |exposedFolder| to |remoteContainer|
  annosvc.setItemAnnotation(remoteContainer, "exposedFolder",
                            exposedFolder, 0, 0);

  // query |remoteContainer|
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([remoteContainer], 1);
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;

  // two nodes should be at the top lop of this container after opening it,
  // the first is an arbitrary uri node ("http://foo.tld/"), the second is a
  // folder node representing |exposedFolder|.
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 2);

  do_check_eq(rootNode.getChild(0).uri, "http://foo.tld/");
  var folder = rootNode.getChild(1).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(folder.itemId, exposedFolder);
  folder.containerOpen = true;
  do_check_eq(folder.childCount, 1);

  // check live update of a folder exposed within a remote container
  bmsvc.insertBookmark(exposedFolder, uri("http://uri2.tld"), bmsvc.DEFAULT_INDEX, "");
  do_check_eq(folder.childCount, 2);

  // Bug 457681
  // Make the dynamic container read-only and check that it appear in the result
  bmsvc.setFolderReadonly(remoteContainer, true);
  options = histsvc.getNewQueryOptions();
  query = histsvc.getNewQuery();
  query.setFolders([testRoot], 1);
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 2);
  do_check_eq(rootNode.getChild(1).title, "remote container sample");
  rootNode.containerOpen = false;

  // Bug 457686
  // If the dynamic container is child of an excludeItems query it should not
  // append uri nodes.
  // The dynamic container should only return an empty folder on opening.
  options = histsvc.getNewQueryOptions();
  options.excludeItems = true;
  query = histsvc.getNewQuery();
  query.setFolders([remoteContainer], 1);
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  folder = rootNode.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(folder.itemId, exposedFolder);
  folder.containerOpen = true;
  do_check_eq(folder.childCount, 0);
  folder.containerOpen = false;
  rootNode.containerOpen = false;
}
