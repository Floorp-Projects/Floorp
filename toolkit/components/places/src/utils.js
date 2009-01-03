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

const POST_DATA_ANNO = "bookmarkProperties/POSTData";
const READ_ONLY_ANNO = "placesInternal/READ_ONLY";
const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";

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
   * Determines if a node is read only (children cannot be inserted, sometimes
   * they cannot be removed depending on the circumstance)
   * @param   aNode
   *          A result node
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PU_nodeIsReadOnly(aNode) {
    if (this.nodeIsFolder(aNode) || this.nodeIsDynamicContainer(aNode))
      return this.bookmarks.getFolderReadonly(this.getConcreteItemId(aNode));
    if (this.nodeIsQuery(aNode) &&
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
  * Determines whether a result node is a remote container registered by the
  * livemark service.
  * @param aNode
  *        A result Node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkContainer: function PU_nodeIsLivemarkContainer(aNode) {
    // Use the annotations service directly to avoid instantiating
    // the Livemark service on startup. (bug 398300)
    return this.nodeIsFolder(aNode) &&
           this.annotations.itemHasAnnotation(aNode.itemId, LMANNO_FEEDURI);
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
      var queries = aNode.getQueries({});
      var folders = queries[0].getFolders({});
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
        self.serializeNodeAsJSONToOutputStream(convertNode(aNode), writer, true, aForceCopy);
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
        return gatherDataUrl(convertNode(aNode));

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
        return gatherDataHtml(convertNode(aNode));
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

    return gatherDataText(convertNode(aNode));
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
    var annoNames = annosvc.getPageAnnotationNames(aURI, {});
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
    var annoNames = annosvc.getItemAnnotationNames(aItemId, {});
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
   */
  setAnnotationsForURI: function PU_setAnnotationsForURI(aURI, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
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
   */
  setAnnotationsForItem: function PU_setAnnotationsForItem(aItemId, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
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

  // identifier getters for special folders
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
        var bookmarks = this.bookmarks.getBookmarkIdsForURI(uri, {});
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
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI, {});

    // filter the ids list
    return bmkIds.filter(function(aID) {
      var parent = this.bookmarks.getFolderIdForItem(aID);
      // Livemark child
      if (this.annotations.itemHasAnnotation(parent, LMANNO_FEEDURI))
        return false;
      var grandparent = this.bookmarks.getFolderIdForItem(parent);
      // item under a tag container
      if (grandparent == this.tagsFolderId)
        return false;
      return true;
    }, this);
  },

  /**
   * Get the most recently added/modified bookmark for a URL, excluding items
   * under tag or livemark containers. -1 is returned if no item is found.
   */
  getMostRecentBookmarkForURI:
  function PU_getMostRecentBookmarkForURI(aURI) {
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI, {});
    for (var i = 0; i < bmkIds.length; i++) {
      // Find the first folder which isn't a tag container
      var bk = bmkIds[i];
      var parent = this.bookmarks.getFolderIdForItem(bk);
      if (parent == this.unfiledBookmarksFolderId)
        return bk;

      var grandparent = this.bookmarks.getFolderIdForItem(parent);
      if (grandparent != this.tagsFolderId &&
          !this.annotations.itemHasAnnotation(parent, LMANNO_FEEDURI))
        return bk;
    }
    return -1;
  },

  getMostRecentFolderForFeedURI:
  function PU_getMostRecentFolderForFeedURI(aURI) {
    var feedSpec = aURI.spec
    var annosvc = this.annotations;
    var livemarks = annosvc.getItemsWithAnnotation(LMANNO_FEEDURI, {});
    for (var i = 0; i < livemarks.length; i++) {
      if (annosvc.getItemAnnotation(livemarks[i], LMANNO_FEEDURI) == feedSpec)
        return livemarks[i];
    }
    return -1;
  },

  // Returns true if a container has uris in its first level
  // Has better performances than checking getURLsForContainerNode(node).length
  hasChildURIs: function PU_hasChildURIs(aNode) {
    if (!this.nodeIsContainer(aNode))
      return false;

    // in the Library left pane we use excludeItems
    if (this.nodeIsFolder(aNode) && asQuery(aNode).queryOptions.excludeItems) {
      var itemId = PlacesUtils.getConcreteItemId(aNode);
      var contents = this.getFolderContents(itemId, false, false).root;
      for (var i = 0; i < contents.childCount; ++i) {
        var child = contents.getChild(i);
        if (this.nodeIsURI(child))
          return true;
      }
      return false;
    }

    var wasOpen = aNode.containerOpen;
    if (!wasOpen)
      aNode.containerOpen = true;
    var found = false;
    for (var i = 0; i < aNode.childCount && !found; i++) {
      var child = aNode.getChild(i);
      if (this.nodeIsURI(child))
        found = true;
    }
    if (!wasOpen)
      aNode.containerOpen = false;
    return found;
  },

  getURLsForContainerNode: function PU_getURLsForContainerNode(aNode) {
    let urls = [];
    if (this.nodeIsFolder(aNode) && asQuery(aNode).queryOptions.excludeItems) {
      // grab manually
      var itemId = this.getConcreteItemId(aNode);
      let contents = this.getFolderContents(itemId, false, false).root;
      for (let i = 0; i < contents.childCount; ++i) {
        let child = contents.getChild(i);
        if (this.nodeIsURI(child))
          urls.push({uri: child.uri, isBookmark: this.nodeIsBookmark(child)});
      }
    }
    else {
      let result, oldViewer, wasOpen;
      try {
        let wasOpen = aNode.containerOpen;
        result = aNode.parentResult;
        oldViewer = result.viewer;
        if (!wasOpen) {
          result.viewer = null;
          aNode.containerOpen = true;
        }
        for (let i = 0; i < aNode.childCount; ++i) {
          // Include visible url nodes only
          let child = aNode.getChild(i);
          if (this.nodeIsURI(child)) {
            // If the node contents is visible, add the uri only if its node is
            // visible. Otherwise follow viewer's collapseDuplicates property,
            // default to true
            if ((wasOpen && oldViewer && child.viewIndex != -1) ||
                (oldViewer && !oldViewer.collapseDuplicates) ||
                urls.indexOf(child.uri) == -1) {
              urls.push({ uri: child.uri,
                          isBookmark: this.nodeIsBookmark(child) });
            }
          }
        }
        if (!wasOpen)
          aNode.containerOpen = false;
      }
      finally {
        if (!wasOpen)
          result.viewer = oldViewer;
      }
    }

    return urls;
  },

  /**
   * Restores bookmarks/tags from a JSON file.
   * WARNING: This method *removes* any bookmarks in the collection before
   * restoring from the file.
   *
   * @param aFile
   *        nsIFile of bookmarks in JSON format to be restored.
   * @param aExcludeItems
   *        Array of root item ids (ie: children of the places root)
   *        to not delete when restoring.
   */
  restoreBookmarksFromJSONFile:
  function PU_restoreBookmarksFromJSONFile(aFile, aExcludeItems) {
    // open file stream
    var stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
    stream.init(aFile, 0x01, 0, 0);
    var converted = Cc["@mozilla.org/intl/converter-input-stream;1"].
                    createInstance(Ci.nsIConverterInputStream);
    converted.init(stream, "UTF-8", 1024,
                   Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

    // read in contents
    var str = {};
    var jsonStr = "";
    while (converted.readString(4096, str) != 0)
      jsonStr += str.value;
    converted.close();

    if (jsonStr.length == 0)
      return; // empty file

    this.restoreBookmarksFromJSONString(jsonStr, true, aExcludeItems);
  },

  /**
   * Import bookmarks from a JSON string.
   * 
   * @param aString
   *        JSON string of serialized bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   * @param aExcludeItems
   *        Array of root item ids (ie: children of the places root)
   *        to not delete when restoring.
   */
  restoreBookmarksFromJSONString:
  function PU_restoreBookmarksFromJSONString(aString, aReplace, aExcludeItems) {
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
          var excludeItems = aExcludeItems || [];
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
            if (excludeItems.indexOf(childId) == -1)
              childIds.push(childId);
          }
          root.containerOpen = false;

          for (var i = 0; i < childIds.length; i++) {
            var rootItemId = childIds[i];
            if (rootItemId == this._utils.tagsFolderId) {
              // remove tags via the tagging service
              var tags = this._utils.tagging.allTags;
              var uris = [];
              var bogusTagContainer = false;
              for (let i in tags) {
                var tagURIs = [];
                // skip empty tags since getURIsForTag would throw
                if (tags[i])
                  tagURIs = this._utils.tagging.getURIsForTag(tags[i]);

                if (!tagURIs.length) {
                  // This is a bogus tag container, empty tags should be removed
                  // automatically, but this does not work if they contain some
                  // not-uri node, so we remove them manually.
                  // XXX this is a temporary workaround until we implement
                  // preventive database maintenance in bug 431558.
                  bogusTagContainer = true;
                }
                for (let j in tagURIs)
                  this._utils.tagging.untagURI(tagURIs[j], [tags[i]]);
              }
              if (bogusTagContainer)
                this._utils.bookmarks.removeFolderChildren(rootItemId);
            }
            else if ([this._utils.toolbarFolderId,
                      this._utils.unfiledBookmarksFolderId,
                      this._utils.bookmarksMenuFolderId].indexOf(rootItemId) != -1)
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
              folderIdMap = folderIdMap.concat(folders);
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
        if (aContainer == PlacesUtils.bookmarks.tagsFolder) {
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
            if (aAnno.name == LMANNO_FEEDURI) {
              feedURI = this._uri(aAnno.value);
              return false;
            }
            else if (aAnno.name == LMANNO_SITEURI) {
              siteURI = this._uri(aAnno.value);
              return false;
            }
            return true;
          }, this);

          if (feedURI)
            id = this.livemarks.createLivemark(aContainer, aData.title, siteURI, feedURI, aIndex);
        }
        else {
          id = this.bookmarks.createFolder(aContainer, aData.title, aIndex);
          folderIdMap.push([aData.id, id]);
          // process children
          if (aData.children) {
            aData.children.every(function(aChild, aIndex) {
              var [folderIds, searches] = this.importJSONNode(aChild, id, aIndex);
              folderIdMap = folderIdMap.concat(folderIds);
              searchIds = searchIds.concat(searches);
              return true;
            }, this);
          }
        }
        break;
      case this.TYPE_X_MOZ_PLACE:
        id = this.bookmarks.insertBookmark(aContainer, this._uri(aData.uri), aIndex, aData.title);
        if (aData.keyword)
          this.bookmarks.setKeywordForBookmark(id, aData.keyword);
        if (aData.tags) {
          var tags = aData.tags.split(", ");
          if (tags.length)
            this.tagging.tagURI(this._uri(aData.uri), tags);
        }
        if (aData.charset)
          this.history.setCharsetForURI(this._uri(aData.uri), aData.charset);
        if (aData.uri.match(/^place:/))
          searchIds.push(id);
        break;
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
        id = this.bookmarks.insertSeparator(aContainer, aIndex);
        break;
      default:
    }

    // set generic properties
    if (id != -1) {
      this.bookmarks.setItemDateAdded(id, aData.dateAdded);
      this.bookmarks.setItemLastModified(id, aData.lastModified);
      if (aData.annos)
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
    var queries = {};
    var options = {};
    this.history.queryStringToQueries(aQueryURI.spec, queries, {}, options);

    var fixedQueries = [];
    queries.value.forEach(function(aQuery) {
      var folders = aQuery.getFolders({});

      var newFolders = [];
      aFolderIdMap.forEach(function(aMapping) {
        if (folders.indexOf(aMapping[0]) != -1)
          newFolders.push(aMapping[1]);
      });

      if (newFolders.length)
        aQuery.setFolders(newFolders, newFolders.length);
      fixedQueries.push(aQuery);
    });

    var stringURI = this.history.queriesToQueryString(fixedQueries,
                                                      fixedQueries.length,
                                                      options.value);
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
      var id = aPlacesNode.itemId;
      if (id != -1) {
        aJSNode.id = id;

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
          annos = self.getAnnotationsForItem(id).filter(function(anno) {
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
      // saved queries
      var concreteId = PlacesUtils.getConcreteItemId(aPlacesNode);
      if (aJSNode.id != -1 && (PlacesUtils.nodeIsQuery(aPlacesNode) ||
          (concreteId != aPlacesNode.itemId && !aResolveShortcuts))) {
        aJSNode.type = self.TYPE_X_MOZ_PLACE;
        aJSNode.uri = aPlacesNode.uri;
        // folder shortcut
        if (aIsUICommand)
          aJSNode.concreteId = concreteId;
        return;
      }
      else if (aJSNode.id != -1) { // bookmark folder
        if (concreteId != aPlacesNode.itemId)
        aJSNode.type = self.TYPE_X_MOZ_PLACE;
        aJSNode.type = self.TYPE_X_MOZ_PLACE_CONTAINER;
        // mark special folders
        if (aJSNode.id == self.bookmarks.placesRoot)
          aJSNode.root = "placesRoot";
        else if (aJSNode.id == self.bookmarks.bookmarksMenuFolder)
          aJSNode.root = "bookmarksMenuFolder";
        else if (aJSNode.id == self.bookmarks.tagsFolder)
          aJSNode.root = "tagsFolder";
        else if (aJSNode.id == self.bookmarks.unfiledBookmarksFolder)
          aJSNode.root = "unfiledBookmarksFolder";
        else if (aJSNode.id == self.bookmarks.toolbarFolder)
          aJSNode.root = "toolbarFolder";
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
   * Serializes bookmarks using JSON, and writes to the supplied file.
   */
  backupBookmarksToFile: function PU_backupBookmarksToFile(aFile, aExcludeItems) {
    if (aFile.exists() && !aFile.isWritable())
      return; // XXX

    // init stream
    var stream = Cc["@mozilla.org/network/file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);
    stream.init(aFile, 0x02 | 0x08 | 0x20, 0600, 0);

    // utf-8 converter stream
    var converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
                 createInstance(Ci.nsIConverterOutputStream);
    converter.init(stream, "UTF-8", 0, 0x0000);

    // weep over stream interface variance
    var streamProxy = {
      converter: converter,
      write: function(aData, aLen) {
        this.converter.writeString(aData);
      }
    };

    // query places root
    var options = this.history.getNewQueryOptions();
    options.expandQueries = false;
    var query = this.history.getNewQuery();
    query.setFolders([this.bookmarks.placesRoot], 1);
    var result = this.history.executeQuery(query, options);
    result.root.containerOpen = true;
    // serialize as JSON, write to stream
    this.serializeNodeAsJSONToOutputStream(result.root, streamProxy,
                                           false, false, aExcludeItems);
    result.root.containerOpen = false;

    // close converter and stream
    converter.close();
    stream.close();
  },

  /**
   * ArchiveBookmarksFile()
   *
   * Creates a dated backup once a day in <profile>/bookmarkbackups.
   * Stores the bookmarks using JSON.
   *
   * @param int aNumberOfBackups - the maximum number of backups to keep
   *
   * @param bool aForceArchive - forces creating an archive even if one was 
   *                             already created that day (overwrites)
   */
  archiveBookmarksFile:
  function PU_archiveBookmarksFile(aNumberOfBackups, aForceArchive) {
    // get/create backups directory
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    var bookmarksBackupDir = dirService.get("ProfD", Ci.nsILocalFile);
    bookmarksBackupDir.append("bookmarkbackups");
    if (!bookmarksBackupDir.exists()) {
      bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
      if (!bookmarksBackupDir.exists())
        return; // unable to create directory!
    }

    // construct the new leafname
    // Use YYYY-MM-DD (ISO 8601) as it doesn't contain illegal characters
    // and makes the alphabetical order of multiple backup files more useful.
    var date = new Date().toLocaleFormat("%Y-%m-%d");
    var backupFilename = "bookmarks-" + date + ".json";

    var backupFile = null;
    if (!aForceArchive) {
      var backupFileNames = [];
      var backupFilenamePrefix = backupFilename.substr(0, backupFilename.indexOf("-"));

      // Get the localized backup filename, to clear out
      // old backups with a localized name (bug 445704).
      var localizedFilename = this.getFormattedString("bookmarksArchiveFilename", [date]);
      var localizedFilenamePrefix = localizedFilename.substr(0, localizedFilename.indexOf("-"));
      var rx = new RegExp("^(bookmarks|" + localizedFilenamePrefix + ")-.+\.(json|html)");

      var entries = bookmarksBackupDir.directoryEntries;
      while (entries.hasMoreElements()) {
        var entry = entries.getNext().QueryInterface(Ci.nsIFile);
        var backupName = entry.leafName;
        // A valid backup is any file that matches either the localized or
        // not-localized filename (bug 445704).
        if (backupName.match(rx)) {
          if (backupName == backupFilename)
            backupFile = entry;
          backupFileNames.push(backupName);
        }
      }

      var numberOfBackupsToDelete = 0;
      if (aNumberOfBackups > -1)
        numberOfBackupsToDelete = backupFileNames.length - aNumberOfBackups;

      if (numberOfBackupsToDelete > 0) {
        // If we don't have today's backup, remove one more so that
        // the total backups after this operation does not exceed the
        // number specified in the pref.
        if (!backupFile)
          numberOfBackupsToDelete++;

        backupFileNames.sort();
        while (numberOfBackupsToDelete--) {
          let backupFile = bookmarksBackupDir.clone();
          backupFile.append(backupFileNames[0]);
          backupFile.remove(false);
          backupFileNames.shift();
        }
      }

      // do nothing if we either have today's backup already
      // or the user has set the pref to zero.
      if (backupFile || aNumberOfBackups == 0)
        return;
    }

    backupFile = bookmarksBackupDir.clone();
    backupFile.append(backupFilename);

    if (aForceArchive && backupFile.exists())
        backupFile.remove(false);

    if (!backupFile.exists())
      backupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);

    this.backupBookmarksToFile(backupFile);
  },

  /**
   * Get the most recent backup file.
   * @returns nsIFile backup file
   */
  getMostRecentBackup: function PU_getMostRecentBackup() {
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    var bookmarksBackupDir = dirService.get("ProfD", Ci.nsILocalFile);
    bookmarksBackupDir.append("bookmarkbackups");
    if (!bookmarksBackupDir.exists())
      return null;

    var backups = [];
    var entries = bookmarksBackupDir.directoryEntries;
    while (entries.hasMoreElements()) {
      var entry = entries.getNext().QueryInterface(Ci.nsIFile);
      if (!entry.isHidden() && entry.leafName.match(/^bookmarks-.+(html|json)?$/))
        backups.push(entry.leafName);
    }

    if (backups.length ==  0)
      return null;

    backups.sort();
    var filename = backups.pop();

    var backupFile = bookmarksBackupDir.clone();
    backupFile.append(filename);
    return backupFile;
  },

  /**
   * Starts the database coherence check and executes update tasks on a timer,
   * this method is called by browser.js in delayed startup.
   */
  startPlacesDBUtils: function PU_startPlacesDBUtils() {
    Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");
  }
};
