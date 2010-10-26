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
 * The Initial Developer of the Original Code is Mozilla Foundation
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

 try {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);
  var ans = Cc["@mozilla.org/browser/annotation-service;1"].
            getService(Ci.nsIAnnotationService);
} catch (ex) {
  do_throw("Could not get services\n");
}

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onItemAdded: function(id, folder, index, itemType, uri) {
    do_check_true(id > 0);
  },
  onBeforeItemRemoved: function() {},
  onItemRemoved: function() {},
  onItemChanged: function() {},
  onItemVisited: function() {},
  onItemMoved: function() {},
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
bms.addObserver(observer, false);

// main
function run_test() {
  // load our dynamic-container sample service
  do_load_manifest("nsDynamicContainerServiceSample.manifest");
  var testRoot = bms.createFolder(bms.placesRoot, "test root", bms.DEFAULT_INDEX);

  var options = hs.getNewQueryOptions();
  var query = hs.getNewQuery();
  query.setFolders([testRoot], 1);
  var result = hs.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // create our dynamic container while the containing folder is open
  var remoteContainer =
    bms.createDynamicContainer(testRoot, "remote container sample",
                                "@mozilla.org/browser/remote-container-sample;1",
                                bms.DEFAULT_INDEX);

  rootNode.containerOpen = false;
}
