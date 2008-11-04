/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is the Places Tagging Service.
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const NH_CONTRACTID = "@mozilla.org/browser/nav-history-service;1";
const BMS_CONTRACTID = "@mozilla.org/browser/nav-bookmarks-service;1";
const IO_CONTRACTID = "@mozilla.org/network/io-service;1";
const ANNO_CONTRACTID = "@mozilla.org/browser/annotation-service;1";
const FAV_CONTRACTID = "@mozilla.org/browser/favicon-service;1";
const OBSS_CONTRACTID = "@mozilla.org/observer-service;1";

var gIoService = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);

/**
 * The Places Tagging Service
 */
function TaggingService() {
}

TaggingService.prototype = {
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc[BMS_CONTRACTID].getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  get _history() {
    if (!this.__history)
      this.__history = Cc[NH_CONTRACTID].getService(Ci.nsINavHistoryService);
    return this.__history;
  },

  get _annos() {
    if (!this.__annos)
      this.__annos =  Cc[ANNO_CONTRACTID].getService(Ci.nsIAnnotationService);
    return this.__annos;
  },

  get _tagsResult() {
    if (!this.__tagsResult) {
      var options = this._history.getNewQueryOptions();
      var query = this._history.getNewQuery();
      query.setFolders([this._bms.tagsFolder], 1);
      this.__tagsResult = this._history.executeQuery(query, options);
      this.__tagsResult.root.containerOpen = true;
      this.__tagsResult.viewer = this;

      // we need to null out the result on shutdown
      var observerSvc = Cc[OBSS_CONTRACTID].getService(Ci.nsIObserverService);
      observerSvc.addObserver(this, "xpcom-shutdown", false);
    }
    return this.__tagsResult;
  },

  // Feed XPCOMUtils
  classDescription: "Places Tagging Service",
  contractID: "@mozilla.org/browser/tagging-service;1",
  classID: Components.ID("{bbc23860-2553-479d-8b78-94d9038334f7}"),

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITaggingService,
                                         Ci.nsIObserver]),

  /**
   * If there's no tag with the given name, null is returned;
   */
  _getTagNode: function TS__getTagIndex(aTagNameOrId) {
    if (!aTagNameOrId)
      throw Cr.NS_ERROR_INVALID_ARG;

    var nameLower = null;
    if (typeof(aTagNameOrId) == "string")
      nameLower = aTagNameOrId.toLowerCase();

    var root = this._tagsResult.root;
    var cc = root.childCount;
    for (var i=0; i < cc; i++) {
      var child = root.getChild(i);
      if ((nameLower && child.title.toLowerCase() == nameLower) ||
          child.itemId === aTagNameOrId)
        return child;
    }

    return null;
  },

  /**
   * Creates a tag container under the tags-root with the given name.
   *
   * @param aName
   *        the name for the new container.
   * @returns the id of the new container.
   */
  _createTag: function TS__createTag(aName) {
    return this._bms.createFolder(this._bms.tagsFolder, aName,
                                  this._bms.DEFAULT_INDEX);
  },

  /**
   * Checks whether the given uri is tagged with the given tag.
   *
   * @param [in] aURI
   *        url to check for
   * @param [in] aTagId
   *        id of the folder representing the tag to check
   * @param [out] aItemId
   *        the id of the item found under the tag container
   * @returns true if the given uri is tagged with the given tag, false
   *          otherwise.
   */
  _isURITaggedInternal: function TS__uriTagged(aURI, aTagId, aItemId) {
    var bookmarkIds = this._bms.getBookmarkIdsForURI(aURI, {});
    for (var i=0; i < bookmarkIds.length; i++) {
      var parent = this._bms.getFolderIdForItem(bookmarkIds[i]);
      if (parent == aTagId) {
        aItemId.value = bookmarkIds[i];
        return true;
      }
    }
    return false;
  },

  // nsITaggingService
  tagURI: function TS_tagURI(aURI, aTags) {
    if (!aURI || !aTags)
      throw Cr.NS_ERROR_INVALID_ARG;

    for (var i=0; i < aTags.length; i++) {
      var tagNode = this._getTagNode(aTags[i]);
      if (!tagNode) {
        if (typeof(aTags[i]) == "number")
          throw Cr.NS_ERROR_INVALID_ARG;

        var tagId = this._createTag(aTags[i]);
        this._bms.insertBookmark(tagId, aURI, this._bms.DEFAULT_INDEX, null);
      }
      else {
        var tagId = tagNode.itemId;
        if (!this._isURITaggedInternal(aURI, tagNode.itemId, {}))
          this._bms.insertBookmark(tagId, aURI, this._bms.DEFAULT_INDEX, null);

        // _getTagNode ignores case sensitivity
        // rename the tag container so the places view would match the
        // user-typed values
        if (typeof(aTags[i]) == "string" && tagNode.title != aTags[i])
          this._bms.setItemTitle(tagNode.itemId, aTags[i]);
      }
    }
  },

  /**
   * Removes the tag container from the tags-root if the given tag is empty.
   *
   * @param aTagId
   *        the item-id of the tag element under the tags root
   */
  _removeTagIfEmpty: function TS__removeTagIfEmpty(aTagId) {
    var node = this._getTagNode(aTagId).QueryInterface(Ci.nsINavHistoryContainerResultNode);
    var wasOpen = node.containerOpen;
    if (!wasOpen)
      node.containerOpen = true;
    var cc = node.childCount;
    if (wasOpen)
      node.containerOpen = false;
    if (cc == 0) {
      this._bms.removeFolder(node.itemId);
    }
  },

  // nsITaggingService
  untagURI: function TS_untagURI(aURI, aTags) {
    if (!aURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    if (!aTags) {
      // see IDL.
      // XXXmano: write a perf-sensitive version of this code path...
      aTags = this.getTagsForURI(aURI, { });
    }

    for (var i=0; i < aTags.length; i++) {
      var tagNode = this._getTagNode(aTags[i]);
      if (tagNode) {
        var itemId = { };
        if (this._isURITaggedInternal(aURI, tagNode.itemId, itemId)) {
          this._bms.removeItem(itemId.value);
          this._removeTagIfEmpty(tagNode.itemId);
        }
      }
      else if (typeof(aTags[i]) == "number")
        throw Cr.NS_ERROR_INVALID_ARG;
    }
  },

  // nsITaggingService
  getURIsForTag: function TS_getURIsForTag(aTag) {
    if (aTag.length == 0)
      throw Cr.NS_ERROR_INVALID_ARG;

    var uris = [];
    var tagNode = this._getTagNode(aTag);
    if (tagNode) {
      tagNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      tagNode.containerOpen = true;
      var cc = tagNode.childCount;
      for (var i = 0; i < cc; i++) {
        try {
          uris.push(gIoService.newURI(tagNode.getChild(i).uri, null, null));
        } catch (ex) {
          // This is an invalid node, tags should only contain valid uri nodes.
          // continue to next node.
        }
      }
      tagNode.containerOpen = false;
    }
    return uris;
  },

  // nsITaggingService
  getTagsForURI: function TS_getTagsForURI(aURI, aCount) {
    if (!aURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    var tags = [];
    var bookmarkIds = this._bms.getBookmarkIdsForURI(aURI, {});
    var root = this._tagsResult.root;
    var cc = root.childCount;
    for (var i=0; i < bookmarkIds.length; i++) {
      var parent = this._bms.getFolderIdForItem(bookmarkIds[i]);
      for (var j=0; j < cc; j++) {
        var child = root.getChild(j);
        if (child.itemId == parent)
          tags.push(child.title);
      }
    }

    // sort the tag list
    tags.sort();
    aCount.value = tags.length;
    return tags;
  },

  // nsITaggingService
  _allTags: null,
  get allTags() {
    if (!this._allTags) {
      this._allTags = [];
      var root = this._tagsResult.root;
      var cc = root.childCount;
      for (var j=0; j < cc; j++) {
        var child = root.getChild(j);
        this._allTags.push(child.title);
      }

      // sort the tag list
      this.allTags.sort();
    }
    return this._allTags;
  },

  // nsIObserver
  observe: function TS_observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this.__tagsResult.root.containerOpen = false;
      this.__tagsResult.viewer = null;
      this.__tagsResult = null;
      var observerSvc = Cc[OBSS_CONTRACTID].getService(Ci.nsIObserverService);
      observerSvc.removeObserver(this, "xpcom-shutdown");
    }
  },

  // nsINavHistoryResultViewer
  // Used to invalidate the cached tag list
  itemInserted: function() this._allTags = null,
  itemRemoved: function() this._allTags = null,
  itemMoved: function() {},
  itemChanged: function() {},
  itemReplaced: function() {},
  containerOpened: function() {},
  containerClosed: function() {},
  invalidateContainer: function() this._allTags = null,
  invalidateAll: function() this._allTags = null,
  sortingChanged: function() {},
  result: null
};

// Implements nsIAutoCompleteResult
function TagAutoCompleteResult(searchString, searchResult,
                               defaultIndex, errorDescription,
                               results, comments) {
  this._searchString = searchString;
  this._searchResult = searchResult;
  this._defaultIndex = defaultIndex;
  this._errorDescription = errorDescription;
  this._results = results;
  this._comments = comments;
}

TagAutoCompleteResult.prototype = {
  
  /**
   * The original search string
   */
  get searchString() {
    return this._searchString;
  },

  /**
   * The result code of this result object, either:
   *         RESULT_IGNORED   (invalid searchString)
   *         RESULT_FAILURE   (failure)
   *         RESULT_NOMATCH   (no matches found)
   *         RESULT_SUCCESS   (matches found)
   */
  get searchResult() {
    return this._searchResult;
  },

  /**
   * Index of the default item that should be entered if none is selected
   */
  get defaultIndex() {
    return this._defaultIndex;
  },

  /**
   * A string describing the cause of a search failure
   */
  get errorDescription() {
    return this._errorDescription;
  },

  /**
   * The number of matches
   */
  get matchCount() {
    return this._results.length;
  },

  /**
   * Get the value of the result at the given index
   */
  getValueAt: function PTACR_getValueAt(index) {
    return this._results[index];
  },

  /**
   * Get the comment of the result at the given index
   */
  getCommentAt: function PTACR_getCommentAt(index) {
    return this._comments[index];
  },

  /**
   * Get the style hint for the result at the given index
   */
  getStyleAt: function PTACR_getStyleAt(index) {
    if (!this._comments[index])
      return null;  // not a category label, so no special styling

    if (index == 0)
      return "suggestfirst";  // category label on first line of results

    return "suggesthint";   // category label on any other line of results
  },

  /**
   * Get the image for the result at the given index
   */
  getImageAt: function PTACR_getImageAt(index) {
    return null;
  },

  /**
   * Remove the value at the given index from the autocomplete results.
   * If removeFromDb is set to true, the value should be removed from
   * persistent storage as well.
   */
  removeValueAt: function PTACR_removeValueAt(index, removeFromDb) {
    this._results.splice(index, 1);
    this._comments.splice(index, 1);
  },

  QueryInterface: function(aIID) {
    if (!aIID.equals(Ci.nsIAutoCompleteResult) && !aIID.equals(Ci.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

// Implements nsIAutoCompleteSearch
function TagAutoCompleteSearch() {
}

TagAutoCompleteSearch.prototype = {
  _stopped : false, 

  get tagging() {
    let svc = Cc["@mozilla.org/browser/tagging-service;1"].
              getService(Ci.nsITaggingService);
    this.__defineGetter__("tagging", function() svc);
    return this.tagging;
  },

  /*
   * Search for a given string and notify a listener (either synchronously
   * or asynchronously) of the result
   *
   * @param searchString - The string to search for
   * @param searchParam - An extra parameter
   * @param previousResult - A previous result to use for faster searching
   * @param listener - A listener to notify when the search is complete
   */
  startSearch: function PTACS_startSearch(searchString, searchParam, result, listener) {
    var searchResults = this.tagging.allTags;
    var results = [];
    var comments = [];
    this._stopped = false;

    // only search on characters for the last tag
    var index = Math.max(searchString.lastIndexOf(","), 
      searchString.lastIndexOf(";"));
    var before = ''; 
    if (index != -1) {  
      before = searchString.slice(0, index+1);
      searchString = searchString.slice(index+1);
      // skip past whitespace
      var m = searchString.match(/\s+/);
      if (m) {
         before += m[0];
         searchString = searchString.slice(m[0].length);
      }
    }

    if (!searchString.length) {
      var newResult = new TagAutoCompleteResult(searchString,
        Ci.nsIAutoCompleteResult.RESULT_NOMATCH, 0, "", results, comments);
      listener.onSearchResult(self, newResult);
      return;
    }
    
    var self = this;
    // generator: if yields true, not done
    function doSearch() {
      var i = 0;
      while (i < searchResults.length) {
        if (self._stopped)
          yield false;
        // for each match, prepend what the user has typed so far
        var pattern = new RegExp("(^" + searchResults[i] + "$|" + searchResults[i] + "(,|;))");
        if (searchResults[i].indexOf(searchString) == 0 &&
            !pattern.test(before)) {
          results.push(before + searchResults[i]);
          comments.push(searchResults[i]);
        }
    
        ++i;
        // 100 loops per yield
        if ((i % 100) == 0) {
          var newResult = new TagAutoCompleteResult(searchString,
            Ci.nsIAutoCompleteResult.RESULT_SUCCESS_ONGOING, 0, "", results, comments);
          listener.onSearchResult(self, newResult);
          yield true;
        } 
      }

      var newResult = new TagAutoCompleteResult(searchString,
        Ci.nsIAutoCompleteResult.RESULT_SUCCESS, 0, "", results, comments);
      listener.onSearchResult(self, newResult);
      yield false;
    }
    
    // chunk the search results via a generator
    var gen = doSearch();
    function driveGenerator() {
      if (gen.next()) { 
        var timer = Cc["@mozilla.org/timer;1"]
          .createInstance(Components.interfaces.nsITimer);
        self._callback = driveGenerator;
        timer.initWithCallback(self, 0, timer.TYPE_ONE_SHOT);
      }
      else {
        gen.close();	
      }
    }
    driveGenerator();
  },

  notify: function PTACS_notify(timer) {
    if (this._callback) 
      this._callback();
  },

  /*
   * Stop an asynchronous search that is in progress
   */
  stopSearch: function PTACS_stopSearch() {
    this._stopped = true;
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch,
                                         Ci.nsITimerCallback]), 

  classDescription: "Places Tag AutoComplete",
  contractID: "@mozilla.org/autocomplete/search;1?name=places-tag-autocomplete",
  classID: Components.ID("{1dcc23b0-d4cb-11dc-9ad6-479d56d89593}")
};

var component = [TaggingService, TagAutoCompleteSearch];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(component);
}
