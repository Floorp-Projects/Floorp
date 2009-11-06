/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Command Controller.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Myk Melez <myk@mozilla.org>
 *   Asaf Romano <mano@mozilla.com>
 *   Sungjoon Steve Won <stevewon@gmail.com>
 *   Dietrich Ayala <dietrich@mozilla.com>
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

function LOG(str) {
  dump("*** " + str + "\n");
}

var EXPORTED_SYMBOLS = ["PlacesUtils"];

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const EXCLUDE_FROM_BACKUP_ANNO = "places/excludeFromBackup";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";
const READ_ONLY_ANNO = "placesInternal/READ_ONLY";
const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";
const LMANNO_EXPIRATION = "livemark/expiration";
const LMANNO_LOADFAILED = "livemark/loadfailed";
const LMANNO_LOADING = "livemark/loading";

// The RESTORE_*_NSIOBSERVER_TOPIC constants should match the #defines of the
// same names in browser/components/places/src/nsPlacesImportExportService.cpp
const RESTORE_BEGIN_NSIOBSERVER_TOPIC = "bookmarks-restore-begin";
const RESTORE_SUCCESS_NSIOBSERVER_TOPIC = "bookmarks-restore-success";
const RESTORE_FAILED_NSIOBSERVER_TOPIC = "bookmarks-restore-failed";
const RESTORE_NSIOBSERVER_DATA = "json";

#ifdef XP_MACOSX
// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where we
// really just want "\n".
const NEWLINE= "\n";
#else
// On other platforms, the transferable system converts "\r\n" to "\n".
const NEWLINE = "\r\n";
#endif

function QI_node(aNode, aIID) {
  var result = null;
  try {
    result = aNode.QueryInterface(aIID);
  }
  catch (e) {
  }
  return result;
}
function asVisit(aNode)    { return QI_node(aNode, Ci.nsINavHistoryVisitResultNode);    }
function asFullVisit(aNode){ return QI_node(aNode, Ci.nsINavHistoryFullVisitResultNode);}
function asContainer(aNode){ return QI_node(aNode, Ci.nsINavHistoryContainerResultNode);}
function asQuery(aNode)    { return QI_node(aNode, Ci.nsINavHistoryQueryResultNode);    }

var PlacesUtils = {
  // Place entries that are containers, e.g. bookmark folders or queries.
  TYPE_X_MOZ_PLACE_CONTAINER: "text/x-moz-place-container",
  // Place entries that are bookmark separators.
  TYPE_X_MOZ_PLACE_SEPARATOR: "text/x-moz-place-separator",
  // Place entries that are not containers or separators
  TYPE_X_MOZ_PLACE: "text/x-moz-place",
  // Place entries in shortcut url format (url\ntitle)
  TYPE_X_MOZ_URL: "text/x-moz-url",
  // Place entries formatted as HTML anchors
  TYPE_HTML: "text/html",
  // Place entries as raw URL text
  TYPE_UNICODE: "text/unicode",

  /**
   * The Bookmarks Service.
   */
  get bookmarks() {
    delete this.bookmarks;
    return this.bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                            getService(Ci.nsINavBookmarksService);
  },

  /**
   * The Nav History Service.
   */
  get history() {
    delete this.history;
    return this.history = Cc["@mozilla.org/browser/nav-history-service;1"].
                          getService(Ci.nsINavHistoryService);
  },

  /**
   * The Live Bookmark Service.
   */
  get livemarks() {
    delete this.livemarks;
    return this.livemarks = Cc["@mozilla.org/browser/livemark-service;2"].
                            getService(Ci.nsILivemarkService);
  },

  /**
   * The Annotations Service.
   */
  get annotations() {
    delete this.annotations;
    return this.annotations = Cc["@mozilla.org/browser/annotation-service;1"].
                              getService(Ci.nsIAnnotationService);
  },

  /**
   * The Favicons Service
   */
  get favicons() {
    delete this.favicons;
    return this.favicons = Cc["@mozilla.org/browser/favicon-service;1"].
                           getService(Ci.nsIFaviconService);
  },

  /**
   * The Places Tagging Service
   */
  get tagging() {
    delete this.tagging;
    return this.tagging = Cc["@mozilla.org/browser/tagging-service;1"].
                          getService(Ci.nsITaggingService);
  },

  get observerSvc() {
    delete this.observerSvc;
    return this.observerSvc = Cc["@mozilla.org/observer-service;1"].
                              getService(Ci.nsIObserverService);
  },

  /**
   * Makes a URI from a spec.
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  _uri: function PU__uri(aSpec) {
    return Cc["@mozilla.org/network/io-service;1"].
           getService(Ci.nsIIOService).
           newURI(aSpec, null, null);
  },

  /**
   * String bundle helpers
   */
  get _bundle() {
    const PLACES_STRING_BUNDLE_URI =
        "chrome://places/locale/places.properties";
    delete this._bundle;
    return this._bundle = Cc["@mozilla.org/intl/stringbundle;1"].
                          getService(Ci.nsIStringBundleService).
                          createBundle(PLACES_STRING_BUNDLE_URI);
  },

  getFormattedString: function PU_getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },

  getString: function PU_getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark folder.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PU_nodeIsFolder(aNode) {
    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER ||
            aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT);
  },

  /**
   * Determines whether or not a ResultNode represents a bookmarked URI.
   * @param   aNode
   *          A result node
   * @returns true if the node represents a bookmarked URI, false otherwise
   */
  nodeIsBookmark: function PU_nodeIsBookmark(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_URI &&
           aNode.itemId != -1;
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark separator.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Bookmark separator, false otherwise
   */
  nodeIsSeparator: function PU_nodeIsSeparator(aNode) {

    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR);
  },

  /**
   * Determines whether or not a ResultNode is a visit item.
   * @param   aNode
   *          A result node
   * @returns true if the node is a visit item, false otherwise
   */
  nodeIsVisit: function PU_nodeIsVisit(aNode) {
    var type = aNode.type;
    return type == Ci.nsINavHistoryResultNode.RESULT_TYPE_VISIT ||
           type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FULL_VISIT;
  },

  /**
   * Determines whether or not a ResultNode is a URL item.
   * @param   aNode
   *          A result node
   * @returns true if the node is a URL item, false otherwise
   */
  uriTypes: [Ci.nsINavHistoryResultNode.RESULT_TYPE_URI,
             Ci.nsINavHistoryResultNode.RESULT_TYPE_VISIT,
             Ci.nsINavHistoryResultNode.RESULT_TYPE_FULL_VISIT],
  nodeIsURI: function PU_nodeIsURI(aNode) {
    return this.uriTypes.indexOf(aNode.type) != -1;
  },

  /**
   * Determines whether or not a ResultNode is a Query item.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Query item, false otherwise
   */
  nodeIsQuery: function PU_nodeIsQuery(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
  },

  /**
   * Cache array of read-only item IDs.
   *
   * The first time this property is called:
   * - the cache is filled with all ids with the RO annotation
   * - an annotation observer is added
   * - a shutdown observer is added
   *
   * When the annotation observer detects annotations added or
   * removed that are the RO annotation name, it adds/removes
   * the ids from the cache.
   *
   * At shutdown, the annotation and shutdown observers are removed.
   */
  get _readOnly() {
    // add annotations observer
    this.annotations.addObserver(this, false);

    // observe shutdown, so we can remove the anno observer
    this.observerSvc.addObserver(this, "xpcom-shutdown", false);

    var readOnly = this.annotations.getItemsWithAnnotation(READ_ONLY_ANNO);
    this.__defineGetter__("_readOnly", function() readOnly);
    return this._readOnly;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAnnotationObserver,
                                         Ci.nsIObserver]),

  // nsIObserver
  observe: function PU_observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this.annotations.removeObserver(this);
      this.observerSvc.removeObserver(this, "xpcom-shutdown");
    }
  },

  // nsIAnnotationObserver
  onItemAnnotationSet: function(aItemId, aAnnotationName) {
    if (aAnnotationName == READ_ONLY_ANNO &&
        this._readOnly.indexOf(aItemId) == -1)
      this._readOnly.push(aItemId);
  },
  onItemAnnotationRemoved: function(aItemId, aAnnotationName) {
    var index = this._readOnly.indexOf(aItemId);
    if (aAnnotationName == READ_ONLY_ANNO && index > -1)
      delete this._readOnly[index];
  },
  onPageAnnotationSet: function(aUri, aAnnotationName) {},
  onPageAnnotationRemoved: function(aUri, aAnnotationName) {},

  /**
   * Determines if a node is read only (children cannot be inserted, sometimes
   * they cannot be removed depending on the circumstance)
   * @param   aNode
   *          A result node
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PU_nodeIsReadOnly(aNode) {
    if (this.nodeIsFolder(aNode) || this.nodeIsDynamicContainer(aNode)) {
      if (this._readOnly.indexOf(aNode.itemId) != -1)
        return true;
    }
    else if (this.nodeIsQuery(aNode) &&
             asQuery(aNode).queryOptions.resultType !=
             Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS)
      return aNode.childrenReadOnly;
    return false;
  },

  /**
   * Determines whether or not a ResultNode is a host container.
   * @param   aNode
   *          A result node
   * @returns true if the node is a host container, false otherwise
   */
  nodeIsHost: function PU_nodeIsHost(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY &&
           aNode.parent &&
           asQuery(aNode.parent).queryOptions.resultType ==
             Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY;
  },

  /**
   * Determines whether or not a ResultNode is a day container.
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a day container, false otherwise
   */
  nodeIsDay: function PU_nodeIsDay(aNode) {
    var resultType;
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY &&
           aNode.parent &&
           ((resultType = asQuery(aNode.parent).queryOptions.resultType) ==
               Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
             resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY);
  },

  /**
   * Determines whether or not a result-node is a tag container.
   * @param   aNode
   *          A result-node
   * @returns true if the node is a tag container, false otherwise
   */
  nodeIsTagQuery: function PU_nodeIsTagQuery(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY &&
           asQuery(aNode).queryOptions.resultType ==
             Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS;
  },

  /**
   * Determines whether or not a ResultNode is a container.
   * @param   aNode
   *          A result node
   * @returns true if the node is a container item, false otherwise
   */
  containerTypes: [Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_DYNAMIC_CONTAINER],
  nodeIsContainer: function PU_nodeIsContainer(aNode) {
    return this.containerTypes.indexOf(aNode.type) != -1;
  },

  /**
   * Determines whether or not a ResultNode is an history related container.
   * @param   node
   *          A result node
   * @returns true if the node is an history related container, false otherwise
   */
  nodeIsHistoryContainer: function PU_nodeIsHistoryContainer(aNode) {
    var resultType;
    return this.nodeIsQuery(aNode) &&
           ((resultType = asQuery(aNode).queryOptions.resultType) ==
              Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY ||
            resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
            resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY ||
            this.nodeIsDay(aNode) ||
            this.nodeIsHost(aNode));
  },

  /**
   * Determines whether or not a result-node is a dynamic-container item.
   * The dynamic container result node type is for dynamically created
   * containers (e.g. for the file browser service where you get your folders
   * in bookmark menus).
   * @param   aNode
   *          A result node
   * @returns true if the node is a dynamic container item, false otherwise
   */
  nodeIsDynamicContainer: function PU_nodeIsDynamicContainer(aNode) {
    if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_DYNAMIC_CONTAINER)
      return true;
    return false;
  },

  /**
   * Determines if a container item id is a livemark.
   * @param aItemId
   *        The id of the potential livemark.
   * @returns true if the item is a livemark.
   */
  itemIsLivemark: function PU_itemIsLivemark(aItemId) {
    // If the Livemark service hasn't yet been initialized then
    // use the annotations service directly to avoid instanciating
    // it on startup. (bug 398300)
    if (this.__lookupGetter__("livemarks"))
      return this.annotations.itemHasAnnotation(aItemId, LMANNO_FEEDURI);
    // If the livemark service has already been instanciated, use it.
    return this.livemarks.isLivemark(aItemId);
  },

  /**
   * Determines whether a result node is a livemark container.
   * @param aNode
   *        A result Node
   * @returns true if the node is a livemark container item
   */
  nodeIsLivemarkContainer: function PU_nodeIsLivemarkContainer(aNode) {
    return this.nodeIsFolder(aNode) && this.itemIsLivemark(aNode.itemId);
  },

 /**
  * Determines whether a result node is a live-bookmark item
  * @param aNode
  *        A result node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkItem: function PU_nodeIsLivemarkItem(aNode) {
    return aNode.parent && this.nodeIsLivemarkContainer(aNode.parent);
  },

  /**
   * Determines whether or not a node is a readonly folder.
   * @param   aNode
   *          The node to test.
   * @returns true if the node is a readonly folder.
  */
  isReadonlyFolder: function(aNode) {
    return this.nodeIsFolder(aNode) &&
           this.bookmarks.getFolderReadonly(asQuery(aNode).folderItemId);
  },

  /**
   * Gets the concrete item-id for the given node. Generally, this is just
   * node.itemId, but for folder-shortcuts that's node.folderItemId.
   */
  getConcreteItemId: function PU_getConcreteItemId(aNode) {
    if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT)
      return asQuery(aNode).folderItemId;
    else if (PlacesUtils.nodeIsTagQuery(aNode)) {
      // RESULTS_AS_TAG_CONTENTS queries are similar to folder shortcuts
      // so we can still get the concrete itemId for them.
      var queries = aNode.getQueries();
      var folders = queries[0].getFolders();
      return folders[0];
    }
    return aNode.itemId;
  },

  /**
   * Gets the index of a node within its parent container
   * @param   aNode
   *          The node to look up
   * @returns The index of the node within its parent container, or -1 if the
   *          node was not found or the node specified has no parent.
   */
  getIndexOfNode: function PU_getIndexOfNode(aNode) {
    var parent = aNode.parent;
    if (!parent)
      return -1;
    var wasOpen = parent.containerOpen;
    var result, oldViewer;
    if (!wasOpen) {
      result = parent.parentResult;
      oldViewer = result.viewer;
      result.viewer = null;
      parent.containerOpen = true;
    }
    var cc = parent.childCount;
    for (var i = 0; i < cc && parent.getChild(i) != aNode; ++i);
    if (!wasOpen) {
      parent.containerOpen = false;
      result.viewer = oldViewer;
    }
    return i < cc ? i : -1;
  },

  /**
   * String-wraps a result node according to the rules of the specified
   * content type.
   * @param   aNode
   *          The Result node to wrap (serialize)
   * @param   aType
   *          The content type to serialize as
   * @param   [optional] aOverrideURI
   *          Used instead of the node's URI if provided.
   *          This is useful for wrapping a container as TYPE_X_MOZ_URL,
   *          TYPE_HTML or TYPE_UNICODE.
   * @param   aForceCopy
   *          Does a full copy, resolving folder shortcuts.
   * @returns A string serialization of the node
   */
  wrapNode: function PU_wrapNode(aNode, aType, aOverrideURI, aForceCopy) {
    var self = this;

    // when wrapping a node, we want all the items, even if the original
    // query options are excluding them.
    // this can happen when copying from the left hand pane of the bookmarks
    // organizer
    function convertNode(cNode) {
      if (self.nodeIsFolder(cNode) && asQuery(cNode).queryOptions.excludeItems) {
        var concreteId = self.getConcreteItemId(cNode);
        return self.getFolderContents(concreteId, false, true).root;
      }
      return cNode;
    }

    switch (aType) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        var writer = {
          value: "",
          write: function PU_wrapNode__write(aStr, aLen) {
            this.value += aStr;
          }
        };
        var node = convertNode(aNode);
        self.serializeNodeAsJSONToOutputStream(node, writer, true, aForceCopy);
        // Convert node could pass an open container node.
        if (self.nodeIsContainer(node))
          node.containerOpen = false;
        return writer.value;
      case this.TYPE_X_MOZ_URL:
        function gatherDataUrl(bNode) {
          if (self.nodeIsLivemarkContainer(bNode)) {
            var siteURI = self.livemarks.getSiteURI(bNode.itemId).spec;
            return siteURI + NEWLINE + bNode.title;
          }
          if (self.nodeIsURI(bNode))
            return (aOverrideURI || bNode.uri) + NEWLINE + bNode.title;
          // ignore containers and separators - items without valid URIs
          return "";
        }
        var node = convertNode(aNode);
        var dataUrl = gatherDataUrl(node);
        // Convert node could pass an open container node.
        if (self.nodeIsContainer(node))
          node.containerOpen = false;
        return dataUrl;
        

      case this.TYPE_HTML:
        function gatherDataHtml(bNode) {
          function htmlEscape(s) {
            s = s.replace(/&/g, "&amp;");
            s = s.replace(/>/g, "&gt;");
            s = s.replace(/</g, "&lt;");
            s = s.replace(/"/g, "&quot;");
            s = s.replace(/'/g, "&apos;");
            return s;
          }
          // escape out potential HTML in the title
          var escapedTitle = bNode.title ? htmlEscape(bNode.title) : "";
          if (self.nodeIsLivemarkContainer(bNode)) {
            var siteURI = self.livemarks.getSiteURI(bNode.itemId).spec;
            return "<A HREF=\"" + siteURI + "\">" + escapedTitle + "</A>" + NEWLINE;
          }
          if (self.nodeIsContainer(bNode)) {
            asContainer(bNode);
            var wasOpen = bNode.containerOpen;
            if (!wasOpen)
              bNode.containerOpen = true;

            var childString = "<DL><DT>" + escapedTitle + "</DT>" + NEWLINE;
            var cc = bNode.childCount;
            for (var i = 0; i < cc; ++i)
              childString += "<DD>"
                             + NEWLINE
                             + gatherDataHtml(bNode.getChild(i))
                             + "</DD>"
                             + NEWLINE;
            bNode.containerOpen = wasOpen;
            return childString + "</DL>" + NEWLINE;
          }
          if (self.nodeIsURI(bNode))
            return "<A HREF=\"" + bNode.uri + "\">" + escapedTitle + "</A>" + NEWLINE;
          if (self.nodeIsSeparator(bNode))
            return "<HR>" + NEWLINE;
          return "";
        }
        var node = convertNode(aNode);
        var dataHtml = gatherDataHtml(node);
        // Convert node could pass an open container node.
        if (self.nodeIsContainer(node))
          node.containerOpen = false;
        return dataHtml;
    }
    // case this.TYPE_UNICODE:
    function gatherDataText(bNode) {
      if (self.nodeIsLivemarkContainer(bNode))
        return self.livemarks.getSiteURI(bNode.itemId).spec;
      if (self.nodeIsContainer(bNode)) {
        asContainer(bNode);
        var wasOpen = bNode.containerOpen;
        if (!wasOpen)
          bNode.containerOpen = true;

        var childString = bNode.title + NEWLINE;
        var cc = bNode.childCount;
        for (var i = 0; i < cc; ++i) {
          var child = bNode.getChild(i);
          var suffix = i < (cc - 1) ? NEWLINE : "";
          childString += gatherDataText(child) + suffix;
        }
        bNode.containerOpen = wasOpen;
        return childString;
      }
      if (self.nodeIsURI(bNode))
        return (aOverrideURI || bNode.uri);
      if (self.nodeIsSeparator(bNode))
        return "--------------------";
      return "";
    }

    var node = convertNode(aNode);
    var dataText = gatherDataText(node);
    // Convert node could pass an open container node.
    if (self.nodeIsContainer(node))
      node.containerOpen = false;
    return dataText;
  },

  /**
   * Unwraps data from the Clipboard or the current Drag Session.
   * @param   blob
   *          A blob (string) of data, in some format we potentially know how
   *          to parse.
   * @param   type
   *          The content type of the blob.
   * @returns An array of objects representing each item contained by the source.
   */
  unwrapNodes: function PU_unwrapNodes(blob, type) {
    // We split on "\n"  because the transferable system converts "\r\n" to "\n"
    var nodes = [];
    switch(type) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        var JSON = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
        nodes = JSON.decode("[" + blob + "]");
        break;
      case this.TYPE_X_MOZ_URL:
        var parts = blob.split("\n");
        // data in this type has 2 parts per entry, so if there are fewer
        // than 2 parts left, the blob is malformed and we should stop
        // but drag and drop of files from the shell has parts.length = 1
        if (parts.length != 1 && parts.length % 2)
          break;
        for (var i = 0; i < parts.length; i=i+2) {
          var uriString = parts[i];
          var titleString = "";
          if (parts.length > i+1)
            titleString = parts[i+1];
          else {
            // for drag and drop of files, try to use the leafName as title
            try {
              titleString = this._uri(uriString).QueryInterface(Ci.nsIURL)
                              .fileName;
            }
            catch (e) {}
          }
          // note:  this._uri() will throw if uriString is not a valid URI
          if (this._uri(uriString)) {
            nodes.push({ uri: uriString,
                         title: titleString ? titleString : uriString ,
                         type: this.TYPE_X_MOZ_URL });
          }
        }
        break;
      case this.TYPE_UNICODE:
        var parts = blob.split("\n");
        for (var i = 0; i < parts.length; i++) {
          var uriString = parts[i];
          // text/uri-list is converted to TYPE_UNICODE but it could contain
          // comments line prepended by #, we should skip them
          if (uriString.substr(0, 1) == '\x23')
            continue;
          // note: this._uri() will throw if uriString is not a valid URI
          if (uriString != "" && this._uri(uriString))
            nodes.push({ uri: uriString,
                         title: uriString,
                         type: this.TYPE_X_MOZ_URL });
        }
        break;
      default:
        LOG("Cannot unwrap data of type " + type);
        throw Cr.NS_ERROR_INVALID_ARG;
    }
    return nodes;
  },

  /**
   * Generates a nsINavHistoryResult for the contents of a folder.
   * @param   folderId
   *          The folder to open
   * @param   [optional] excludeItems
   *          True to hide all items (individual bookmarks). This is used on
   *          the left places pane so you just get a folder hierarchy.
   * @param   [optional] expandQueries
   *          True to make query items expand as new containers. For managing,
   *          you want this to be false, for menus and such, you want this to
   *          be true.
   * @returns A nsINavHistoryResult containing the contents of the
   *          folder. The result.root is guaranteed to be open.
   */
  getFolderContents:
  function PU_getFolderContents(aFolderId, aExcludeItems, aExpandQueries) {
    var query = this.history.getNewQuery();
    query.setFolders([aFolderId], 1);
    var options = this.history.getNewQueryOptions();
    options.excludeItems = aExcludeItems;
    options.expandQueries = aExpandQueries;

    var result = this.history.executeQuery(query, options);
    result.root.containerOpen = true;
    return result;
  },

  /**
   * Fetch all annotations for a URI, including all properties of each
   * annotation which would be required to recreate it.
   * @param aURI
   *        The URI for which annotations are to be retrieved.
   * @return Array of objects, each containing the following properties:
   *         name, flags, expires, mimeType, type, value
   */
  getAnnotationsForURI: function PU_getAnnotationsForURI(aURI) {
    var annosvc = this.annotations;
    var annos = [], val = null;
    var annoNames = annosvc.getPageAnnotationNames(aURI);
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, mimeType = {}, storageType = {};
      annosvc.getPageAnnotationInfo(aURI, annoNames[i], flags, exp, mimeType, storageType);
      if (storageType.value == annosvc.TYPE_BINARY) {
        var data = {}, length = {}, mimeType = {};
        annosvc.getPageAnnotationBinary(aURI, annoNames[i], data, length, mimeType);
        val = data.value;
      }
      else
        val = annosvc.getPageAnnotation(aURI, annoNames[i]);

      annos.push({name: annoNames[i],
                  flags: flags.value,
                  expires: exp.value,
                  mimeType: mimeType.value,
                  type: storageType.value,
                  value: val});
    }
    return annos;
  },

  /**
   * Fetch all annotations for an item, including all properties of each
   * annotation which would be required to recreate it.
   * @param aItemId
   *        The identifier of the itme for which annotations are to be
   *        retrieved.
   * @return Array of objects, each containing the following properties:
   *         name, flags, expires, mimeType, type, value
   */
  getAnnotationsForItem: function PU_getAnnotationsForItem(aItemId) {
    var annosvc = this.annotations;
    var annos = [], val = null;
    var annoNames = annosvc.getItemAnnotationNames(aItemId);
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, mimeType = {}, storageType = {};
      annosvc.getItemAnnotationInfo(aItemId, annoNames[i], flags, exp, mimeType, storageType);
      if (storageType.value == annosvc.TYPE_BINARY) {
        var data = {}, length = {}, mimeType = {};
        annosvc.geItemAnnotationBinary(aItemId, annoNames[i], data, length, mimeType);
        val = data.value;
      }
      else
        val = annosvc.getItemAnnotation(aItemId, annoNames[i]);

      annos.push({name: annoNames[i],
                  flags: flags.value,
                  expires: exp.value,
                  mimeType: mimeType.value,
                  type: storageType.value,
                  value: val});
    }
    return annos;
  },

  /**
   * Annotate a URI with a batch of annotations.
   * @param aURI
   *        The URI for which annotations are to be set.
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires, type, mimeType (only used for binary
   *        annotations) value.
   *        If the value for an annotation is not set it will be removed.
   */
  setAnnotationsForURI: function PU_setAnnotationsForURI(aURI, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
      if (!anno.value) {
        annosvc.removePageAnnotation(aURI, anno.name);
        return;
      }
      var flags = ("flags" in anno) ? anno.flags : 0;
      var expires = ("expires" in anno) ?
        anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
      if (anno.type == annosvc.TYPE_BINARY) {
        annosvc.setPageAnnotationBinary(aURI, anno.name, anno.value,
                                        anno.value.length, anno.mimeType,
                                        flags, expires);
      }
      else
        annosvc.setPageAnnotation(aURI, anno.name, anno.value, flags, expires);
    });
  },

  /**
   * Annotate an item with a batch of annotations.
   * @param aItemId
   *        The identifier of the item for which annotations are to be set
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires, type, mimeType (only used for binary
   *        annotations) value.
   *        If the value for an annotation is not set it will be removed.
   */
  setAnnotationsForItem: function PU_setAnnotationsForItem(aItemId, aAnnos) {
    var annosvc = this.annotations;

    aAnnos.forEach(function(anno) {
      if (!anno.value) {
        annosvc.removeItemAnnotation(aItemId, anno.name);
        return;
      }
      var flags = ("flags" in anno) ? anno.flags : 0;
      var expires = ("expires" in anno) ?
        anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
      if (anno.type == annosvc.TYPE_BINARY) {
        annosvc.setItemAnnotationBinary(aItemId, anno.name, anno.value,
                                        anno.value.length, anno.mimeType,
                                        flags, expires);
      }
      else {
        annosvc.setItemAnnotation(aItemId, anno.name, anno.value, flags,
                                  expires);
      }
    });
  },

  // Identifier getters for special folders.
  // You should use these everywhere PlacesUtils is available to avoid XPCOM
  // traversal just to get roots' ids.
  get placesRootId() {
    delete this.placesRootId;
    return this.placesRootId = this.bookmarks.placesRoot;
  },

  get bookmarksMenuFolderId() {
    delete this.bookmarksMenuFolderId;
    return this.bookmarksMenuFolderId = this.bookmarks.bookmarksMenuFolder;
  },

  get toolbarFolderId() {
    delete this.toolbarFolderId;
    return this.toolbarFolderId = this.bookmarks.toolbarFolder;
  },

  get tagsFolderId() {
    delete this.tagsFolderId;
    return this.tagsFolderId = this.bookmarks.tagsFolder;
  },

  get unfiledBookmarksFolderId() {
    delete this.unfiledBookmarksFolderId;
    return this.unfiledBookmarksFolderId = this.bookmarks.unfiledBookmarksFolder;
  },

  /**
   * Checks if aItemId is a root.
   *
   *   @param aItemId
   *          item id to look for.
   *   @returns true if aItemId is a root, false otherwise.
   */
  isRootItem: function PU_isRootItem(aItemId) {
    return aItemId == PlacesUtils.bookmarksMenuFolderId ||
           aItemId == PlacesUtils.toolbarFolderId ||
           aItemId == PlacesUtils.unfiledBookmarksFolderId ||
           aItemId == PlacesUtils.tagsFolderId ||
           aItemId == PlacesUtils.placesRootId;
  },

  /**
   * Set the POST data associated with a bookmark, if any.
   * Used by POST keywords.
   *   @param aBookmarkId
   *   @returns string of POST data
   */
  setPostDataForBookmark: function PU_setPostDataForBookmark(aBookmarkId, aPostData) {
    const annos = this.annotations;
    if (aPostData)
      annos.setItemAnnotation(aBookmarkId, POST_DATA_ANNO, aPostData, 
                              0, Ci.nsIAnnotationService.EXPIRE_NEVER);
    else if (annos.itemHasAnnotation(aBookmarkId, POST_DATA_ANNO))
      annos.removeItemAnnotation(aBookmarkId, POST_DATA_ANNO);
  },

  /**
   * Get the POST data associated with a bookmark, if any.
   * @param aBookmarkId
   * @returns string of POST data if set for aBookmarkId. null otherwise.
   */
  getPostDataForBookmark: function PU_getPostDataForBookmark(aBookmarkId) {
    const annos = this.annotations;
    if (annos.itemHasAnnotation(aBookmarkId, POST_DATA_ANNO))
      return annos.getItemAnnotation(aBookmarkId, POST_DATA_ANNO);

    return null;
  },

  /**
   * Get the URI (and any associated POST data) for a given keyword.
   * @param aKeyword string keyword
   * @returns an array containing a string URL and a string of POST data
   */
  getURLAndPostDataForKeyword: function PU_getURLAndPostDataForKeyword(aKeyword) {
    var url = null, postdata = null;
    try {
      var uri = this.bookmarks.getURIForKeyword(aKeyword);
      if (uri) {
        url = uri.spec;
        var bookmarks = this.bookmarks.getBookmarkIdsForURI(uri);
        for (let i = 0; i < bookmarks.length; i++) {
          var bookmark = bookmarks[i];
          var kw = this.bookmarks.getKeywordForBookmark(bookmark);
          if (kw == aKeyword) {
            postdata = this.getPostDataForBookmark(bookmark);
            break;
          }
        }
      }
    } catch(ex) {}
    return [url, postdata];
  },

  /**
   * Get all bookmarks for a URL, excluding items under tag or livemark
   * containers.
   */
  getBookmarksForURI:
  function PU_getBookmarksForURI(aURI) {
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI);

    // filter the ids list
    return bmkIds.filter(function(aID) {
      var parentId = this.bookmarks.getFolderIdForItem(aID);
      // Livemark child
      if (this.itemIsLivemark(parentId))
        return false;
      var grandparentId = this.bookmarks.getFolderIdForItem(parentId);
      // item under a tag container
      if (grandparentId == this.tagsFolderId)
        return false;
      return true;
    }, this);
  },

  /**
   * Get the most recently added/modified bookmark for a URL, excluding items
   * under tag or livemark containers.
   *
   * @param aURI
   *        nsIURI of the page we will look for.
   * @returns itemId of the found bookmark, or -1 if nothing is found.
   */
  getMostRecentBookmarkForURI:
  function PU_getMostRecentBookmarkForURI(aURI) {
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI);
    for (var i = 0; i < bmkIds.length; i++) {
      // Find the first folder which isn't a tag container
      var itemId = bmkIds[i];
      var parentId = this.bookmarks.getFolderIdForItem(itemId);
      // Optimization: if this is a direct child of a root we don't need to
      // check if its grandparent is a tag.
      if (parentId == this.unfiledBookmarksFolderId ||
          parentId == this.toolbarFolderId ||
          parentId == this.bookmarksMenuFolderId)
        return itemId;

      var grandparentId = this.bookmarks.getFolderIdForItem(parentId);
      if (grandparentId != this.tagsFolderId &&
          !this.itemIsLivemark(parentId))
        return itemId;
    }
    return -1;
  },

  /**
   * Get the most recent folder item id for a feed URI.
   *
   * @param aURI
   *        nsIURI of the feed we will look for.
   * @returns folder item id of the found livemark, or -1 if nothing is found.
   */
  getMostRecentFolderForFeedURI:
  function PU_getMostRecentFolderForFeedURI(aFeedURI) {
    // If the Livemark service hasn't yet been initialized then
    // use the annotations service directly to avoid instanciating
    // it on startup. (bug 398300)
    if (this.__lookupGetter__("livemarks")) {
      var feedSpec = aFeedURI.spec
      var annosvc = this.annotations;
      var livemarks = annosvc.getItemsWithAnnotation(LMANNO_FEEDURI);
      for (var i = 0; i < livemarks.length; i++) {
        if (annosvc.getItemAnnotation(livemarks[i], LMANNO_FEEDURI) == feedSpec)
          return livemarks[i];
      }
    }
    else {
      // If the livemark service has already been instanciated, use it.
      return this.livemarks.getLivemarkIdForFeedURI(aFeedURI);
    }

    return -1;
  },

  /**
   * Returns a nsNavHistoryContainerResultNode with forced excludeItems and
   * expandQueries.
   * @param   aNode
   *          The node to convert
   * @param   [optional] excludeItems
   *          True to hide all items (individual bookmarks). This is used on
   *          the left places pane so you just get a folder hierarchy.
   * @param   [optional] expandQueries
   *          True to make query items expand as new containers. For managing,
   *          you want this to be false, for menus and such, you want this to
   *          be true.
   * @returns A nsINavHistoryContainerResultNode containing the unfiltered
   *          contents of the container.
   * @note    The returned container node could be open or closed, we don't
   *          guarantee its status.
   */
  getContainerNodeWithOptions:
  function PU_getContainerNodeWithOptions(aNode, aExcludeItems, aExpandQueries) {
    if (!this.nodeIsContainer(aNode))
      throw Cr.NS_ERROR_INVALID_ARG;

    // excludeItems is inherited by child containers in an excludeItems view.
    var excludeItems = asQuery(aNode).queryOptions.excludeItems ||
                       asQuery(aNode.parentResult.root).queryOptions.excludeItems;
    // expandQueries is inherited by child containers in an expandQueries view.
    var expandQueries = asQuery(aNode).queryOptions.expandQueries &&
                        asQuery(aNode.parentResult.root).queryOptions.expandQueries;

    // If our options are exactly what we expect, directly return the node.
    if (excludeItems == aExcludeItems && expandQueries == aExpandQueries)
      return aNode;

    // Otherwise, get contents manually.
    var queries = {}, options = {};
    this.history.queryStringToQueries(aNode.uri, queries, {}, options);
    options.value.excludeItems = aExcludeItems;
    options.value.expandQueries = aExpandQueries;
    return this.history.executeQueries(queries.value,
                                       queries.value.length,
                                       options.value).root;
  },

  /**
   * Returns true if a container has uri nodes in its first level.
   * Has better performance than (getURLsForContainerNode(node).length > 0).
   * @param aNode
   *        The container node to search through.
   * @returns true if the node contains uri nodes, false otherwise.
   */
  hasChildURIs: function PU_hasChildURIs(aNode) {
    if (!this.nodeIsContainer(aNode))
      return false;

    var root = this.getContainerNodeWithOptions(aNode, false, true);
    var oldViewer = root.parentResult.viewer;
    var wasOpen = root.containerOpen;
    if (!wasOpen) {
      root.parentResult.viewer = null;
      root.containerOpen = true;
    }

    var found = false;
    for (var i = 0; i < root.childCount && !found; i++) {
      var child = root.getChild(i);
      if (this.nodeIsURI(child))
        found = true;
    }

    if (!wasOpen) {
      root.containerOpen = false;
      root.parentResult.viewer = oldViewer;
    }
    return found;
  },

  /**
   * Returns an array containing all the uris in the first level of the
   * passed in container.
   * If you only need to know if the node contains uris, use hasChildURIs.
   * @param aNode
   *        The container node to search through
   * @returns array of uris in the first level of the container.
   */
  getURLsForContainerNode: function PU_getURLsForContainerNode(aNode) {
    var urls = [];
    if (!this.nodeIsContainer(aNode))
      return urls;

    var root = this.getContainerNodeWithOptions(aNode, false, true);
    var oldViewer = root.parentResult.viewer;
    var wasOpen = root.containerOpen;
    if (!wasOpen) {
      root.parentResult.viewer = null;
      root.containerOpen = true;
    }

   for (var i = 0; i < root.childCount; ++i) {
      var child = root.getChild(i);
      if (this.nodeIsURI(child))
        urls.push({uri: child.uri, isBookmark: this.nodeIsBookmark(child)});
    }

    if (!wasOpen) {
      root.containerOpen = false;
      root.parentResult.viewer = oldViewer;
    }
    return urls;
  },

  /**
   * Import bookmarks from a JSON string.
   * Note: any item annotated with "places/excludeFromBackup" won't be removed
   *       before executing the restore.
   * 
   * @param aString
   *        JSON string of serialized bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   */
  restoreBookmarksFromJSONString:
  function PU_restoreBookmarksFromJSONString(aString, aReplace) {
    // convert string to JSON
    var nodes = this.unwrapNodes(aString, this.TYPE_X_MOZ_PLACE_CONTAINER);

    if (nodes.length == 0 || !nodes[0].children ||
        nodes[0].children.length == 0)
      return; // nothing to restore

    // ensure tag folder gets processed last
    nodes[0].children.sort(function sortRoots(aNode, bNode) {
      return (aNode.root && aNode.root == "tagsFolder") ? 1 :
              (bNode.root && bNode.root == "tagsFolder") ? -1 : 0;
    });

    var batch = {
      _utils: this,
      nodes: nodes[0].children,
      runBatched: function restore_runBatched() {
        if (aReplace) {
          // Get roots excluded from the backup, we will not remove them
          // before restoring.
          var excludeItems = this._utils.annotations
                                 .getItemsWithAnnotation(EXCLUDE_FROM_BACKUP_ANNO);
          // delete existing children of the root node, excepting:
          // 1. special folders: delete the child nodes
          // 2. tags folder: untag via the tagging api
          var query = this._utils.history.getNewQuery();
          query.setFolders([this._utils.placesRootId], 1);
          var options = this._utils.history.getNewQueryOptions();
          options.expandQueries = false;
          var root = this._utils.history.executeQuery(query, options).root;
          root.containerOpen = true;
          var childIds = [];
          for (var i = 0; i < root.childCount; i++) {
            var childId = root.getChild(i).itemId;
            if (excludeItems.indexOf(childId) == -1 &&
                childId != this._utils.tagsFolderId)
              childIds.push(childId);
          }
          root.containerOpen = false;

          for (var i = 0; i < childIds.length; i++) {
            var rootItemId = childIds[i];
            if (this._utils.isRootItem(rootItemId))
              this._utils.bookmarks.removeFolderChildren(rootItemId);
            else
              this._utils.bookmarks.removeItem(rootItemId);
          }
        }

        var searchIds = [];
        var folderIdMap = [];

        this.nodes.forEach(function(node) {
          if (!node.children || node.children.length == 0)
            return; // nothing to restore for this root

          if (node.root) {
            var container = this.placesRootId; // default to places root
            switch (node.root) {
              case "bookmarksMenuFolder":
                container = this.bookmarksMenuFolderId;
                break;
              case "tagsFolder":
                container = this.tagsFolderId;
                break;
              case "unfiledBookmarksFolder":
                container = this.unfiledBookmarksFolderId;
                break;
              case "toolbarFolder":
                container = this.toolbarFolderId;
                break;
            }
 
            // insert the data into the db
            node.children.forEach(function(child) {
              var index = child.index;
              var [folders, searches] = this.importJSONNode(child, container, index);
              for (var i = 0; i < folders.length; i++) {
                if (folders[i])
                  folderIdMap[i] = folders[i];
              }
              searchIds = searchIds.concat(searches);
            }, this);
          }
          else
            this.importJSONNode(node, this.placesRootId, node.index);

        }, this._utils);

        // fixup imported place: uris that contain folders
        searchIds.forEach(function(aId) {
          var oldURI = this.bookmarks.getBookmarkURI(aId);
          var uri = this._fixupQuery(this.bookmarks.getBookmarkURI(aId),
                                     folderIdMap);
          if (!uri.equals(oldURI))
            this.bookmarks.changeBookmarkURI(aId, uri);
        }, this._utils);
      }
    };

    this.bookmarks.runInBatchMode(batch, null);
  },

  /**
   * Takes a JSON-serialized node and inserts it into the db.
   *
   * @param   aData
   *          The unwrapped data blob of dropped or pasted data.
   * @param   aContainer
   *          The container the data was dropped or pasted into
   * @param   aIndex
   *          The index within the container the item was dropped or pasted at
   * @returns an array containing of maps of old folder ids to new folder ids,
   *          and an array of saved search ids that need to be fixed up.
   *          eg: [[[oldFolder1, newFolder1]], [search1]]
   */
  importJSONNode: function PU_importJSONNode(aData, aContainer, aIndex) {
    var folderIdMap = [];
    var searchIds = [];
    var id = -1;
    switch (aData.type) {
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        if (aContainer == PlacesUtils.tagsFolderId) {
          // node is a tag
          if (aData.children) {
            aData.children.forEach(function(aChild) {
              try {
                this.tagging.tagURI(this._uri(aChild.uri), [aData.title]);
              } catch (ex) {
                // invalid tag child, skip it
              }
            }, this);
            return [folderIdMap, searchIds];
          }
        }
        else if (aData.livemark && aData.annos) {
          // node is a livemark
          var feedURI = null;
          var siteURI = null;
          aData.annos = aData.annos.filter(function(aAnno) {
            switch (aAnno.name) {
              case LMANNO_FEEDURI:
                feedURI = this._uri(aAnno.value);
                return false;
              case LMANNO_SITEURI:
                siteURI = this._uri(aAnno.value);
                return false;
              case LMANNO_EXPIRATION:
              case LMANNO_LOADING:
              case LMANNO_LOADFAILED:
                return false;
              default:
                return true;
            }
          }, this);

          if (feedURI) {
            id = this.livemarks.createLivemarkFolderOnly(aContainer,
                                                         aData.title,
                                                         siteURI, feedURI,
                                                         aIndex);
          }
        }
        else {
          id = this.bookmarks.createFolder(aContainer, aData.title, aIndex);
          folderIdMap[aData.id] = id;
          // process children
          if (aData.children) {
            aData.children.forEach(function(aChild, aIndex) {
              var [folders, searches] = this.importJSONNode(aChild, id, aIndex);
              for (var i = 0; i < folders.length; i++) {
                if (folders[i])
                  folderIdMap[i] = folders[i];
              }
              searchIds = searchIds.concat(searches);
            }, this);
          }
        }
        break;
      case this.TYPE_X_MOZ_PLACE:
        id = this.bookmarks.insertBookmark(aContainer, this._uri(aData.uri),
                                           aIndex, aData.title);
        if (aData.keyword)
          this.bookmarks.setKeywordForBookmark(id, aData.keyword);
        if (aData.tags) {
          var tags = aData.tags.split(", ");
          if (tags.length)
            this.tagging.tagURI(this._uri(aData.uri), tags);
        }
        if (aData.charset)
          this.history.setCharsetForURI(this._uri(aData.uri), aData.charset);
        if (aData.uri.substr(0, 6) == "place:")
          searchIds.push(id);
        if (aData.icon) {
          try {
            // Create a fake faviconURI to use (FIXME: bug 523932)
            let faviconURI = this._uri("fake-favicon-uri:" + aData.uri);
            this.favicons.setFaviconUrlForPage(this._uri(aData.uri), faviconURI);
            this.favicons.setFaviconDataFromDataURL(faviconURI, aData.icon, 0);
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon data:"  + ex);
          }
        }
        if (aData.iconUri) {
          try {
            this.favicons.setAndLoadFaviconForPage(this._uri(aData.uri),
                                                   this._uri(aData.iconUri),
                                                   false);
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon URI:"  + ex);
          }
        }
        break;
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
        id = this.bookmarks.insertSeparator(aContainer, aIndex);
        break;
      default:
        // Unknown node type
    }

    // set generic properties, valid for all nodes
    if (id != -1) {
      if (aData.dateAdded)
        this.bookmarks.setItemDateAdded(id, aData.dateAdded);
      if (aData.lastModified)
        this.bookmarks.setItemLastModified(id, aData.lastModified);
      if (aData.annos && aData.annos.length)
        this.setAnnotationsForItem(id, aData.annos);
    }

    return [folderIdMap, searchIds];
  },

  /**
   * Replaces imported folder ids with their local counterparts in a place: URI.
   *
   * @param   aURI
   *          A place: URI with folder ids.
   * @param   aFolderIdMap
   *          An array mapping old folder id to new folder ids.
   * @returns the fixed up URI if all matched. If some matched, it returns
   *          the URI with only the matching folders included. If none matched it
   *          returns the input URI unchanged.
   */
  _fixupQuery: function PU__fixupQuery(aQueryURI, aFolderIdMap) {
    function convert(str, p1, offset, s) {
      return "folder=" + aFolderIdMap[p1];
    }
    var stringURI = aQueryURI.spec.replace(/folder=([0-9]+)/g, convert);
    return this._uri(stringURI);
  },

  /**
   * Serializes the given node (and all its descendents) as JSON
   * and writes the serialization to the given output stream.
   * 
   * @param   aNode
   *          An nsINavHistoryResultNode
   * @param   aStream
   *          An nsIOutputStream. NOTE: it only uses the write(str, len)
   *          method of nsIOutputStream. The caller is responsible for
   *          closing the stream.
   * @param   aIsUICommand
   *          Boolean - If true, modifies serialization so that each node self-contained.
   *          For Example, tags are serialized inline with each bookmark.
   * @param   aResolveShortcuts
   *          Converts folder shortcuts into actual folders. 
   * @param   aExcludeItems
   *          An array of item ids that should not be written to the backup.
   */
  serializeNodeAsJSONToOutputStream:
  function PU_serializeNodeAsJSONToOutputStream(aNode, aStream, aIsUICommand,
                                                aResolveShortcuts,
                                                aExcludeItems) {
    var self = this;
    
    function addGenericProperties(aPlacesNode, aJSNode) {
      aJSNode.title = aPlacesNode.title;
      aJSNode.id = aPlacesNode.itemId;
      if (aJSNode.id != -1) {
        var parent = aPlacesNode.parent;
        if (parent)
          aJSNode.parent = parent.itemId;
        var dateAdded = aPlacesNode.dateAdded;
        if (dateAdded)
          aJSNode.dateAdded = dateAdded;
        var lastModified = aPlacesNode.lastModified;
        if (lastModified)
          aJSNode.lastModified = lastModified;

        // XXX need a hasAnnos api
        var annos = [];
        try {
          annos = self.getAnnotationsForItem(aJSNode.id).filter(function(anno) {
            // XXX should whitelist this instead, w/ a pref for
            // backup/restore of non-whitelisted annos
            // XXX causes JSON encoding errors, so utf-8 encode
            //anno.value = unescape(encodeURIComponent(anno.value));
            if (anno.name == LMANNO_FEEDURI)
              aJSNode.livemark = 1;
            if (anno.name == READ_ONLY_ANNO && aResolveShortcuts) {
              // When copying a read-only node, remove the read-only annotation.
              return false;
            }
            return true;
          });
        } catch(ex) {
          LOG(ex);
        }
        if (annos.length != 0)
          aJSNode.annos = annos;
      }
      // XXXdietrich - store annos for non-bookmark items
    }

    function addURIProperties(aPlacesNode, aJSNode) {
      aJSNode.type = self.TYPE_X_MOZ_PLACE;
      aJSNode.uri = aPlacesNode.uri;
      if (aJSNode.id && aJSNode.id != -1) {
        // harvest bookmark-specific properties
        var keyword = self.bookmarks.getKeywordForBookmark(aJSNode.id);
        if (keyword)
          aJSNode.keyword = keyword;
      }

      var tags = aIsUICommand ? aPlacesNode.tags : null;
      if (tags)
        aJSNode.tags = tags;

      // last character-set
      var uri = self._uri(aPlacesNode.uri);
      var lastCharset = self.history.getCharsetForURI(uri);
      if (lastCharset)
        aJSNode.charset = lastCharset;
    }

    function addSeparatorProperties(aPlacesNode, aJSNode) {
      aJSNode.type = self.TYPE_X_MOZ_PLACE_SEPARATOR;
    }

    function addContainerProperties(aPlacesNode, aJSNode) {
      var concreteId = PlacesUtils.getConcreteItemId(aPlacesNode);
      if (concreteId != -1) {
        // This is a bookmark or a tag container.
        if (PlacesUtils.nodeIsQuery(aPlacesNode) ||
            (concreteId != aPlacesNode.itemId && !aResolveShortcuts)) {
          aJSNode.type = self.TYPE_X_MOZ_PLACE;
          aJSNode.uri = aPlacesNode.uri;
          // folder shortcut
          if (aIsUICommand)
            aJSNode.concreteId = concreteId;
        }
        else { // Bookmark folder or a shortcut we should convert to folder.
          aJSNode.type = self.TYPE_X_MOZ_PLACE_CONTAINER;

          // Mark root folders.
          if (aJSNode.id == self.placesRootId)
            aJSNode.root = "placesRoot";
          else if (aJSNode.id == self.bookmarksMenuFolderId)
            aJSNode.root = "bookmarksMenuFolder";
          else if (aJSNode.id == self.tagsFolderId)
            aJSNode.root = "tagsFolder";
          else if (aJSNode.id == self.unfiledBookmarksFolderId)
            aJSNode.root = "unfiledBookmarksFolder";
          else if (aJSNode.id == self.toolbarFolderId)
            aJSNode.root = "toolbarFolder";
        }
      }
      else {
        // This is a grouped container query, generated on the fly.
        aJSNode.type = self.TYPE_X_MOZ_PLACE;
        aJSNode.uri = aPlacesNode.uri;
      }
    }

    function writeScalarNode(aStream, aNode) {
      // serialize to json
      var jstr = self.toJSONString(aNode);
      // write to stream
      aStream.write(jstr, jstr.length);
    }

    function writeComplexNode(aStream, aNode, aSourceNode) {
      var escJSONStringRegExp = /(["\\])/g;
      // write prefix
      var properties = [];
      for (let [name, value] in Iterator(aNode)) {
        if (name == "annos")
          value = self.toJSONString(value);
        else if (typeof value == "string")
          value = "\"" + value.replace(escJSONStringRegExp, '\\$1') + "\"";
        properties.push("\"" + name.replace(escJSONStringRegExp, '\\$1') + "\":" + value);
      }
      var jStr = "{" + properties.join(",") + ",\"children\":[";
      aStream.write(jStr, jStr.length);

      // write child nodes
      if (!aNode.livemark) {
        asContainer(aSourceNode);
        var wasOpen = aSourceNode.containerOpen;
        if (!wasOpen)
          aSourceNode.containerOpen = true;
        var cc = aSourceNode.childCount;
        for (var i = 0; i < cc; ++i) {
          var childNode = aSourceNode.getChild(i);
          if (aExcludeItems && aExcludeItems.indexOf(childNode.itemId) != -1)
            continue;
          var written = serializeNodeToJSONStream(aSourceNode.getChild(i), i);
          if (written && i < cc - 1)
            aStream.write(",", 1);
        }
        if (!wasOpen)
          aSourceNode.containerOpen = false;
      }

      // write suffix
      aStream.write("]}", 2);
    }

    function serializeNodeToJSONStream(bNode, aIndex) {
      var node = {};

      // set index in order received
      // XXX handy shortcut, but are there cases where we don't want
      // to export using the sorting provided by the query?
      if (aIndex)
        node.index = aIndex;

      addGenericProperties(bNode, node);

      var parent = bNode.parent;
      var grandParent = parent ? parent.parent : null;

      if (self.nodeIsURI(bNode)) {
        // Tag root accept only folder nodes
        if (parent && parent.itemId == self.tagsFolderId)
          return false;
        // Check for url validity, since we can't halt while writing a backup.
        // This will throw if we try to serialize an invalid url and it does
        // not make sense saving a wrong or corrupt uri node.
        try {
          self._uri(bNode.uri);
        } catch (ex) {
          return false;
        }
        addURIProperties(bNode, node);
      }
      else if (self.nodeIsContainer(bNode)) {
        // Tag containers accept only uri nodes
        if (grandParent && grandParent.itemId == self.tagsFolderId)
          return false;
        addContainerProperties(bNode, node);
      }
      else if (self.nodeIsSeparator(bNode)) {
        // Tag root accept only folder nodes
        // Tag containers accept only uri nodes
        if ((parent && parent.itemId == self.tagsFolderId) ||
            (grandParent && grandParent.itemId == self.tagsFolderId))
          return false;

        addSeparatorProperties(bNode, node);
      }

      if (!node.feedURI && node.type == self.TYPE_X_MOZ_PLACE_CONTAINER)
        writeComplexNode(aStream, node, bNode);
      else
        writeScalarNode(aStream, node);
      return true;
    }

    // serialize to stream
    serializeNodeToJSONStream(aNode, null);
  },

  /**
   * Serialize a JS object to JSON
   */
  toJSONString: function PU_toJSONString(aObj) {
    var JSON = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    return JSON.encode(aObj);
  },

  /**
   * Restores bookmarks and tags from a JSON file.
   * WARNING: This method *removes* any bookmarks in the collection before
   * restoring from the file.
   *
   * @param aFile
   *        nsIFile of bookmarks in JSON format to be restored.
   */
  restoreBookmarksFromJSONFile:
  function PU_restoreBookmarksFromJSONFile(aFile) {
    let failed = false;
    this.observerSvc.notifyObservers(null,
                                     RESTORE_BEGIN_NSIOBSERVER_TOPIC,
                                     RESTORE_NSIOBSERVER_DATA);

    try {
      // open file stream
      var stream = Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(Ci.nsIFileInputStream);
      stream.init(aFile, 0x01, 0, 0);
      var converted = Cc["@mozilla.org/intl/converter-input-stream;1"].
                      createInstance(Ci.nsIConverterInputStream);
      converted.init(stream, "UTF-8", 8192,
                     Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

      // read in contents
      var str = {};
      var jsonStr = "";
      while (converted.readString(8192, str) != 0)
        jsonStr += str.value;
      converted.close();

      if (jsonStr.length == 0)
        return; // empty file

      this.restoreBookmarksFromJSONString(jsonStr, true);
    }
    catch (exc) {
      failed = true;
      this.observerSvc.notifyObservers(null,
                                       RESTORE_FAILED_NSIOBSERVER_TOPIC,
                                       RESTORE_NSIOBSERVER_DATA);
      Cu.reportError("Bookmarks JSON restore failed: " + exc);
      throw exc;
    }
    finally {
      if (!failed) {
        this.observerSvc.notifyObservers(null,
                                         RESTORE_SUCCESS_NSIOBSERVER_TOPIC,
                                         RESTORE_NSIOBSERVER_DATA);
      }
    }
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file.
   *
   * @see backups.saveBookmarksToJSONFile(aFile)
   */
  backupBookmarksToFile: function PU_backupBookmarksToFile(aFile) {
    this.backups.saveBookmarksToJSONFile(aFile);
  },

  /**
   * Helper to create and manage backups.
   */
  backups: {

    get _filenamesRegex() {
      // Get the localized backup filename, will be used to clear out
      // old backups with a localized name (bug 445704).
      let localizedFilename =
        PlacesUtils.getFormattedString("bookmarksArchiveFilename", [new Date()]);
      let localizedFilenamePrefix =
        localizedFilename.substr(0, localizedFilename.indexOf("-"));
      delete this._filenamesRegex;
      return this._filenamesRegex =
        new RegExp("^(bookmarks|" + localizedFilenamePrefix + ")-([0-9-]+)\.(json|html)");
    },

    get folder() {
      let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
      let bookmarksBackupDir = dirSvc.get("ProfD", Ci.nsILocalFile);
      bookmarksBackupDir.append("bookmarkbackups");
      if (!bookmarksBackupDir.exists()) {
        bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
        if (!bookmarksBackupDir.exists())
          throw("Unable to create bookmarks backup folder");
      }
      delete this.folder;
      return this.folder = bookmarksBackupDir;
    },

    /**
     * Cache current backups in a sorted (by date DESC) array.
     */
    get entries() {
      delete this.entries;
      this.entries = [];
      let files = this.folder.directoryEntries;
      while (files.hasMoreElements()) {
        let entry = files.getNext().QueryInterface(Ci.nsIFile);
        // A valid backup is any file that matches either the localized or
        // not-localized filename (bug 445704).
        let matches = entry.leafName.match(this._filenamesRegex);
        if (!entry.isHidden() && matches) {
          // Remove bogus backups in future dates.
          if (this.getDateForFile(entry) > new Date()) {
            entry.remove(false);
            continue;
          }
          this.entries.push(entry);
        }
      }
      this.entries.sort(function compare(a, b) {
        let aDate = PlacesUtils.backups.getDateForFile(a);
        let bDate = PlacesUtils.backups.getDateForFile(b);
        return aDate < bDate ? 1 : aDate > bDate ? -1 : 0;
      });
      return this.entries;
    },

    /**
     * Creates a filename for bookmarks backup files.
     *
     * @param [optional] aDateObj
     *                   Date object used to build the filename.
     *                   Will use current date if empty.
     * @return A bookmarks backup filename.
     */
    getFilenameForDate:
    function PU_B_getFilenameForDate(aDateObj) {
      let dateObj = aDateObj || new Date();
      // Use YYYY-MM-DD (ISO 8601) as it doesn't contain illegal characters
      // and makes the alphabetical order of multiple backup files more useful.
      return "bookmarks-" + dateObj.toLocaleFormat("%Y-%m-%d") + ".json";
    },

    /**
     * Creates a Date object from a backup file.  The date is the backup
     * creation date.
     *
     * @param aBackupFile
     *        nsIFile of the backup.
     * @return A Date object for the backup's creation time.
     */
    getDateForFile:
    function PU_B_getDateForFile(aBackupFile) {
      let filename = aBackupFile.leafName;
      let matches = filename.match(this._filenamesRegex);
      if (!matches)
        do_throw("Invalid backup file name: " + filename);
      return new Date(matches[2].replace(/-/g, "/"));
    },

    /**
     * Get the most recent backup file.
     *
     * @param [optional] aFileExt
     *                   Force file extension.  Either "html" or "json".
     *                   Will check for both if not defined.
     * @returns nsIFile backup file
     */
    getMostRecent:
    function PU__B_getMostRecent(aFileExt) {
      let fileExt = aFileExt || "(json|html)";
      for (let i = 0; i < this.entries.length; i++) {
        let rx = new RegExp("\." + fileExt + "$");
        if (this.entries[i].leafName.match(rx))
          return this.entries[i];
      }
      return null;
    },

    /**
     * saveBookmarksToJSONFile()
     *
     * Serializes bookmarks using JSON, and writes to the supplied file.
     * Note: any item that should not be backed up must be annotated with
     *       "places/excludeFromBackup".
     *
     * @param aFile
     *        nsIFile where to save JSON backup.
     */
    saveBookmarksToJSONFile:
    function PU_B_saveBookmarksToFile(aFile) {
      if (!aFile.exists())
        aFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
      if (!aFile.exists() || !aFile.isWritable()) {
        Cu.reportError("Unable to create bookmarks backup file: " + aFile.leafName);
        return;
      }

      this._writeBackupFile(aFile);

      if (aFile.parent.equals(this.folder)) {
        // Update internal cache.
        this.entries.push(aFile);
      }
      else {
        // If we are saving to a folder different than our backups folder, then
        // we also want to copy this new backup to it.
        // This way we ensure the latest valid backup is the same saved by the
        // user.  See bug 424389.
        var latestBackup = this.getMostRecent("json");
        if (!latestBackup || latestBackup != aFile) {
          let name = this.getFilenameForDate();
          let file = this.folder.clone();
          file.append(name);
          if (file.exists())
            file.remove(false);
          else {
            // Update internal cache if we are not replacing an existing
            // backup file.
            this.entries.push(file);
          }
          aFile.copyTo(this.folder, name);
        }
      }
    },

    _writeBackupFile:
    function PU_B__writeBackupFile(aFile) {
      // Init stream.
      let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
      stream.init(aFile, 0x02 | 0x08 | 0x20, 0600, 0);

      // UTF-8 converter stream.
      let converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
                   createInstance(Ci.nsIConverterOutputStream);
      converter.init(stream, "UTF-8", 0, 0x0000);

      // Weep over stream interface variance.
      let streamProxy = {
        converter: converter,
        write: function(aData, aLen) {
          this.converter.writeString(aData);
        }
      };

      // Get list of itemIds that must be exluded from the backup.
      let excludeItems =
        PlacesUtils.annotations.getItemsWithAnnotation(EXCLUDE_FROM_BACKUP_ANNO);

      // Query the Places root.
      let options = PlacesUtils.history.getNewQueryOptions();
      options.expandQueries = false;
      let query = PlacesUtils.history.getNewQuery();
      query.setFolders([PlacesUtils.placesRootId], 1);
      let root = PlacesUtils.history.executeQuery(query, options).root;
      root.containerOpen = true;
      // Serialize to JSON and write to stream.
      PlacesUtils.serializeNodeAsJSONToOutputStream(root, streamProxy,
                                                    false, false, excludeItems);
      root.containerOpen = false;

      // Close converter and stream.
      converter.close();
      stream.close();
    },

    /**
     * create()
     *
     * Creates a dated backup in <profile>/bookmarkbackups.
     * Stores the bookmarks using JSON.
     * Note: any item that should not be backed up must be annotated with
     *       "places/excludeFromBackup".
     *
     * @param [optional] int aMaxBackups
     *                       The maximum number of backups to keep.
     *
     * @param [optional] bool aForceBackup
     *                        Forces creating a backup even if one was already
     *                        created that day (overwrites).
     */
    create:
    function PU_B_create(aMaxBackups, aForceBackup) {
      // Construct the new leafname.
      let newBackupFilename = this.getFilenameForDate();
      let mostRecentBackupFile = this.getMostRecent();

      if (!aForceBackup) {
        let numberOfBackupsToDelete = 0;
        if (aMaxBackups !== undefined && aMaxBackups > -1)
          numberOfBackupsToDelete = this.entries.length - aMaxBackups;

        if (numberOfBackupsToDelete > 0) {
          // If we don't have today's backup, remove one more so that
          // the total backups after this operation does not exceed the
          // number specified in the pref.
          if (!mostRecentBackupFile ||
              mostRecentBackupFile.leafName != newBackupFilename)
            numberOfBackupsToDelete++;

          while (numberOfBackupsToDelete--) {
            let oldestBackup = this.entries.pop();
            oldestBackup.remove(false);
          }
        }

        // Do nothing if we already have this backup or we don't want backups.
        if (aMaxBackups === 0 ||
            (mostRecentBackupFile &&
             mostRecentBackupFile.leafName == newBackupFilename))
          return;
      }

      let newBackupFile = this.folder.clone();
      newBackupFile.append(newBackupFilename);

      if (aForceBackup && newBackupFile.exists())
        newBackupFile.remove(false);

      if (newBackupFile.exists())
        return;

      this.saveBookmarksToJSONFile(newBackupFile);
    }

  },

  /**
   * Starts the database coherence check and executes update tasks on a timer,
   * this method is called by browser.js in delayed startup.
   */
  startPlacesDBUtils: function PU_startPlacesDBUtils() {
    Cu.import("resource://gre/modules/PlacesDBUtils.jsm");
  }
};
