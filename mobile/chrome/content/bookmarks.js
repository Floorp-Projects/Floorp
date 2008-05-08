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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

var Bookmarks = {
  bookmarks : null,
  panel : null,
  item : null,

  edit : function(aURI) {
    this.bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    this.panel = document.getElementById("bookmark_edit");
    this.panel.hidden = false; // was initially hidden to reduce Ts

    var bookmarkIDs = this.bookmarks.getBookmarkIdsForURI(aURI, {});
    if (bookmarkIDs.length > 0) {
      this.item = bookmarkIDs[0];
      document.getElementById("bookmark_url").value = aURI.spec;
      document.getElementById("bookmark_name").value = this.bookmarks.getItemTitle(this.item);

      this.panel.openPopup(document.getElementById("tool_star"), "after_end", 0, 0, false, false);
    }
  },

  remove : function() {
    if (this.item) {
      this.bookmarks.removeItem(this.item);
      document.getElementById("tool_star").removeAttribute("starred");
    }
    this.close();
  },

  save : function() {
    if (this.panel && this.item) {
      var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
      var bookmarkURI = ios.newURI(document.getElementById("bookmark_url").value, null, null);
      if (bookmarkURI) {
        this.bookmarks.setItemTitle(this.item, document.getElementById("bookmark_name").value);
        this.bookmarks.changeBookmarkURI(this.item, bookmarkURI);
      }
    }
    this.close();
  },

  close : function() {
    if (this.panel) {
      this.item = null;
      this.panel.hidePopup();
      this.panel = null;
    }
  },

  list : function() {
    this.bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    this.panel = document.getElementById("bookmark_picker");
    this.panel.hidden = false; // was initially hidden to reduce Ts

    var list = document.getElementById("bookmark_list");
    while (list.childNodes.length > 0) {
      list.removeChild(list.childNodes[0]);
    }

    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);

    var items = this.getBookmarks();
    if (items.length > 0) {
      for (var i=0; i<items.length; i++) {
        var itemId = items[i];
        var listItem = document.createElement("richlistitem");
        listItem.setAttribute("class", "bookmarklist-item");

        var box = document.createElement("vbox");
        box.setAttribute("pack", "center");
        var image = document.createElement("image");
        image.setAttribute("class", "bookmarklist-image");
        image.setAttribute("src", fis.getFaviconImageForPage(this.bookmarks.getBookmarkURI(itemId)).spec);
        box.appendChild(image);
        listItem.appendChild(box);

        var label = document.createElement("label");
        label.setAttribute("class", "bookmarklist-text");
        label.setAttribute("value", this.bookmarks.getItemTitle(itemId));
        label.setAttribute("flex", "1");
        label.setAttribute("crop", "end");
        label.setAttribute("onclick", "Bookmarks.open(" + itemId + ");");
        listItem.appendChild(label);
        list.appendChild(listItem);
      }
      this.panel.openPopup(document.getElementById("tool_bookmarks"), "after_end", 0, 0, false, false);
    }
  },

  open : function(aItem) {
    var bookmarkURI = this.bookmarks.getBookmarkURI(aItem);
    getBrowser().loadURI(bookmarkURI.spec, null, null, false);
    this.close();
  },

  getBookmarks : function() {
    var items = [];

    var history = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    var options = history.getNewQueryOptions();
    var query = history.getNewQuery();
    query.setFolders([this.bookmarks.bookmarksMenuFolder], 1);
    var result = history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i<cc; ++i) {
      var node = rootNode.getChild(i);
      if (node.type == node.RESULT_TYPE_URI) {
        items.push(node.itemId);
      }
    }
    rootNode.containerOpen = false;

    return items;
  }
};
