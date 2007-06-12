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
 * The Original Code is the Places Tagging Service
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
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

const TAGS_CLASSID = Components.ID("{6a059068-1630-11dc-8314-0800200c9a66}");
const TAGS_CLASSNAME = "Places Tagging Service";
const TAGS_CONTRACTID = "@mozilla.org/browser/tagging-service;1";

const TAG_CONTAINER_ICON_URI = "chrome://mozapps/skin/places/tagContainerIcon.png"

const NH_CONTRACTID = "@mozilla.org/browser/nav-history-service;1";
const BMS_CONTRACTID = "@mozilla.org/browser/nav-bookmarks-service;1";
const IO_CONTRACTID = "@mozilla.org/network/io-service;1";
const FAV_CONTRACTID = "@mozilla.org/browser/favicon-service;1";

var gIoService = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);

/**
 * The Places Tagging Service
 */
function TaggingService() {
  this._tags = [];
  
  var options = this._history.getNewQueryOptions();
  var query = this._history.getNewQuery();
  query.setFolders([this._bms.tagRoot], 1);
  var result = this._history.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  var cc = rootNode.childCount;
  for (var i=0; i < cc; i++) {
    var child = rootNode.getChild(i);
    this._tags.push({ itemId: child.itemId, name: child.title });
  }
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

  // nsISupports
  QueryInterface: function TS_QueryInterface(iid) {
    if (iid.equals(Ci.nsITaggingService) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NO_INTERFACE;
  },

  /**
   * If there's no tag with the given name, -1 is returned.
   */
  _getTagIndex: function TS__getTagIndex(aName) {
    for (var i=0; i < this._tags.length; i++) {
      if (this._tags[i].name == aName)
        return i;
    }

    return -1;
  },

  get _tagContainerIcon() {
    if (!this.__tagContainerIcon) {
      this.__tagContainerIcon =
        gIoService.newURI(TAG_CONTAINER_ICON_URI, null, null);
    }

    return this.__tagContainerIcon;
  },

  /**
   * Creates a tag container under the tags-root with the given name
   *
   * @param aName
   *        the name for the new container.
   * @returns the id of the new container.
   */
  _createTag: function TS__createTag(aName) {
    var id = this._bms.createFolder(this._bms.tagRoot, aName,
                                    this._bms.DEFAULT_INDEX);
    this._tags.push({ itemId: id, name: aName});

    // Set the favicon
    var faviconService = Cc[FAV_CONTRACTID].getService(Ci.nsIFaviconService);
    var uri = this._bms.getFolderURI(id);
    faviconService.setFaviconUrlForPage(uri, this._tagContainerIcon);
  
    return id;
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
    var options = this._history.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var query = this._history.getNewQuery();
    query.setFolders([aTagId], 1);
    query.uri = aURI;
    var result = this._history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    if (rootNode.childCount != 0) {
      aItemId.value = rootNode.getChild(0).itemId;
      return true;
    }
    return false;
  },

  // nsITaggingService
  tagURI: function TS_tagURI(aURI, aTags, aCount) {
    if (!aURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    for (var i=0; i < aTags.length; i++) {
      if (aTags[i].length == 0)
        throw Cr.NS_ERROR_INVALID_ARG;

      var tagIndex = this._getTagIndex(aTags[i]);
      if (tagIndex == -1) {
        var tagId = this._createTag(aTags[i]);
        this._bms.insertBookmark(tagId, aURI, this._bms.DEFAULT_INDEX, "");
      }
      else {
        var tagId = this._tags[tagIndex].itemId;
        if (!this._isURITaggedInternal(aURI, tagId, {}))
          this._bms.insertBookmark(tagId, aURI, this._bms.DEFAULT_INDEX, "");
      }
    }
  },

  /**
   * Removes the tag container from the tags-root if the given tag is empty.
   *
   * @param aTagIndex
   *        the index of a tag element under the _tags array
   */
  _removeTagAtIndexIfEmpty: function TS__removeTagAtIndexIfEmpty(aTagIndex) {
    var options = this._history.getNewQueryOptions();
    var query = this._history.getNewQuery();
    query.setFolders([this._tags[aTagIndex].itemId], 1);
    var result = this._history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    if (rootNode.childCount == 0) {
      this._bms.removeFolder(this._tags[aTagIndex].itemId);
      this._tags.splice(aTagIndex, 1);
    }
  },

  // nsITaggingService
  untagURI: function TS_untagURI(aURI, aTags, aCount) {
    if (!aURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    for (var i=0; i < aTags.length; i++) {
      if (aTags[i].length == 0)
        throw Cr.NS_ERROR_INVALID_ARG;

      var tagIndex = this._getTagIndex(aTags[i]);
      if (tagIndex != -1) {
        var itemId = { };
        if (this._isURITaggedInternal(aURI, this._tags[tagIndex].itemId,
                                      itemId)) {
          this._bms.removeItem(itemId.value);
          this._removeTagAtIndexIfEmpty(tagIndex);
        }
      }
    }
  },

  // nsITaggingService
  getURIsForTag: function TS_getURIsForTag(aTag) {
    if (aTag.length == 0)
      throw Cr.NS_ERROR_INVALID_ARG;

    var uris = [];
    var tagIndex = this._getTagIndex(aTag);
    if (tagIndex != -1) {
      var tagId = this._tags[tagIndex].itemId;
      var options = this._history.getNewQueryOptions();
      var query = this._history.getNewQuery();
      query.setFolders([tagId], 1);
      var result = this._history.executeQuery(query, options);
      var rootNode = result.root;
      rootNode.containerOpen = true;
      var cc = rootNode.childCount;
      for (var i=0; i < cc; i++)
        uris.push(gIoService.newURI(rootNode.getChild(i).uri, null, null));
    }
    return uris;
  },

  // nsITaggingService
  getTagsForURI: function TS_getTagsForURI(aURI) {
    if (!aURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    var tags = [];

    var bookmarkIds = this._bms.getBookmarkIdsForURI(aURI, {});
    for (var i=0; i < bookmarkIds.length; i++) {
      var parent = this._bms.getFolderIdForItem(bookmarkIds[i]);
      for (var j=0; j < this._tags.length; j++) {
        if (this._tags[j].itemId == parent)
          tags.push(this._tags[j].name);
      }
    }
    return tags;
  }
};


var gModule = {
  registerSelf: function(componentManager, fileSpec, location, type) {
    componentManager = componentManager.QueryInterface(Ci.nsIComponentRegistrar);
    
    for (var key in this._objects) {
      var obj = this._objects[key];
      componentManager.registerFactoryLocation(obj.CID,
                                               obj.className,
                                               obj.contractID,
                                               fileSpec,
                                               location,
                                               type);
    }
  },
  
  unregisterSelf: function(componentManager, fileSpec, location) {},

  getClassObject: function(componentManager, cid, iid) {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  
    for (var key in this._objects) {
      if (cid.equals(this._objects[key].CID))
        return this._objects[key].factory;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  _objects: {
    service: {
      CID        : TAGS_CLASSID,
      contractID : TAGS_CONTRACTID,
      className  : TAGS_CLASSNAME,
      factory    : TaggingServiceFactory = {
                     createInstance: function(aOuter, aIID) {
                       if (aOuter != null)
                         throw Cr.NS_ERROR_NO_AGGREGATION;
                       var svc = new TaggingService();
                      return svc.QueryInterface(aIID);
                     }
                   }
    }
  },

  canUnload: function(componentManager) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return gModule;
}
