/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "PlacesUtils"
, "PlacesAggregatedTransaction"
, "PlacesCreateFolderTransaction"
, "PlacesCreateBookmarkTransaction"
, "PlacesCreateSeparatorTransaction"
, "PlacesCreateLivemarkTransaction"
, "PlacesMoveItemTransaction"
, "PlacesRemoveItemTransaction"
, "PlacesEditItemTitleTransaction"
, "PlacesEditBookmarkURITransaction"
, "PlacesSetItemAnnotationTransaction"
, "PlacesSetPageAnnotationTransaction"
, "PlacesEditBookmarkKeywordTransaction"
, "PlacesEditBookmarkPostDataTransaction"
, "PlacesEditItemDateAddedTransaction"
, "PlacesEditItemLastModifiedTransaction"
, "PlacesSortFolderByNameTransaction"
, "PlacesTagURITransaction"
, "PlacesUntagURITransaction"
];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");

// The minimum amount of transactions before starting a batch. Usually we do
// do incremental updates, a batch will cause views to completely
// refresh instead.
const MIN_TRANSACTIONS_FOR_BATCH = 5;

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
function asContainer(aNode) QI_node(aNode, Ci.nsINavHistoryContainerResultNode);
function asQuery(aNode) QI_node(aNode, Ci.nsINavHistoryQueryResultNode);

this.PlacesUtils = {
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
  // Used to track the action that populated the clipboard.
  TYPE_X_MOZ_PLACE_ACTION: "text/x-moz-place-action",

  EXCLUDE_FROM_BACKUP_ANNO: "places/excludeFromBackup",
  LMANNO_FEEDURI: "livemark/feedURI",
  LMANNO_SITEURI: "livemark/siteURI",
  POST_DATA_ANNO: "bookmarkProperties/POSTData",
  READ_ONLY_ANNO: "placesInternal/READ_ONLY",
  CHARSET_ANNO: "URIProperties/characterSet",

  TOPIC_SHUTDOWN: "places-shutdown",
  TOPIC_INIT_COMPLETE: "places-init-complete",
  TOPIC_DATABASE_LOCKED: "places-database-locked",
  TOPIC_EXPIRATION_FINISHED: "places-expiration-finished",
  TOPIC_FEEDBACK_UPDATED: "places-autocomplete-feedback-updated",
  TOPIC_FAVICONS_EXPIRED: "places-favicons-expired",
  TOPIC_VACUUM_STARTING: "places-vacuum-starting",
  TOPIC_BOOKMARKS_RESTORE_BEGIN: "bookmarks-restore-begin",
  TOPIC_BOOKMARKS_RESTORE_SUCCESS: "bookmarks-restore-success",
  TOPIC_BOOKMARKS_RESTORE_FAILED: "bookmarks-restore-failed",

  asContainer: function(aNode) asContainer(aNode),
  asQuery: function(aNode) asQuery(aNode),

  endl: NEWLINE,

  /**
   * Makes a URI from a spec.
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  _uri: function PU__uri(aSpec) {
    return NetUtil.newURI(aSpec);
  },

  /**
   * Wraps a string in a nsISupportsString wrapper.
   * @param   aString
   *          The string to wrap.
   * @returns A nsISupportsString object containing a string.
   */
  toISupportsString: function PU_toISupportsString(aString) {
    let s = Cc["@mozilla.org/supports-string;1"].
            createInstance(Ci.nsISupportsString);
    s.data = aString;
    return s;
  },

  getFormattedString: function PU_getFormattedString(key, params) {
    return bundle.formatStringFromName(key, params, params.length);
  },

  getString: function PU_getString(key) {
    return bundle.GetStringFromName(key);
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
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR;
  },

  /**
   * Determines whether or not a ResultNode is a URL item.
   * @param   aNode
   *          A result node
   * @returns true if the node is a URL item, false otherwise
   */
  nodeIsURI: function PU_nodeIsURI(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;
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
   * Generator for a node's ancestors.
   * @param aNode
   *        A result node
   */
  nodeAncestors: function PU_nodeAncestors(aNode) {
    let node = aNode.parent;
    while (node) {
      yield node;
      node = node.parent;
    }
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
    // Add annotations observer.
    this.annotations.addObserver(this, false);
    this.registerShutdownFunction(function () {
      this.annotations.removeObserver(this);
    });

    var readOnly = this.annotations.getItemsWithAnnotation(this.READ_ONLY_ANNO);
    this.__defineGetter__("_readOnly", function() readOnly);
    return this._readOnly;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAnnotationObserver
  , Ci.nsIObserver
  , Ci.nsITransactionListener
  ]),

  _shutdownFunctions: [],
  registerShutdownFunction: function PU_registerShutdownFunction(aFunc)
  {
    // If this is the first registered function, add the shutdown observer.
    if (this._shutdownFunctions.length == 0) {
      Services.obs.addObserver(this, this.TOPIC_SHUTDOWN, false);
    }
    this._shutdownFunctions.push(aFunc);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver
  observe: function PU_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case this.TOPIC_SHUTDOWN:
        Services.obs.removeObserver(this, this.TOPIC_SHUTDOWN);
        while (this._shutdownFunctions.length > 0) {
          this._shutdownFunctions.shift().apply(this);
        }
        if (this._bookmarksServiceObserversQueue.length > 0) {
          // Since we are shutting down, there's no reason to add the observers.
          this._bookmarksServiceObserversQueue.length = 0;
        }
        break;
      case "bookmarks-service-ready":
        this._bookmarksServiceReady = true;
        while (this._bookmarksServiceObserversQueue.length > 0) {
          let observerInfo = this._bookmarksServiceObserversQueue.shift();
          this.bookmarks.addObserver(observerInfo.observer, observerInfo.weak);
        }
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAnnotationObserver

  onItemAnnotationSet: function PU_onItemAnnotationSet(aItemId, aAnnotationName)
  {
    if (aAnnotationName == this.READ_ONLY_ANNO &&
        this._readOnly.indexOf(aItemId) == -1)
      this._readOnly.push(aItemId);
  },

  onItemAnnotationRemoved:
  function PU_onItemAnnotationRemoved(aItemId, aAnnotationName)
  {
    var index = this._readOnly.indexOf(aItemId);
    if (aAnnotationName == this.READ_ONLY_ANNO && index > -1)
      delete this._readOnly[index];
  },

  onPageAnnotationSet: function() {},
  onPageAnnotationRemoved: function() {},


  //////////////////////////////////////////////////////////////////////////////
  //// nsITransactionListener

  didDo: function PU_didDo(aManager, aTransaction, aDoResult)
  {
    updateCommandsOnActiveWindow();
  },

  didUndo: function PU_didUndo(aManager, aTransaction, aUndoResult)
  {
    updateCommandsOnActiveWindow();
  },

  didRedo: function PU_didRedo(aManager, aTransaction, aRedoResult)
  {
    updateCommandsOnActiveWindow();
  },

  didBeginBatch: function PU_didBeginBatch(aManager, aResult)
  {
    // A no-op transaction is pushed to the stack, in order to make safe and
    // easy to implement "Undo" an unknown number of transactions (including 0),
    // "above" beginBatch and endBatch. Otherwise,implementing Undo that way
    // head to dataloss: for example, if no changes were done in the
    // edit-item panel, the last transaction on the undo stack would be the
    // initial createItem transaction, or even worse, the batched editing of
    // some other item.
    // DO NOT MOVE this to the window scope, that would leak (bug 490068)!
    this.transactionManager.doTransaction({ doTransaction: function() {},
                                            undoTransaction: function() {},
                                            redoTransaction: function() {},
                                            isTransient: false,
                                            merge: function() { return false; }
                                          });
  },

  willDo: function PU_willDo() {},
  willUndo: function PU_willUndo() {},
  willRedo: function PU_willRedo() {},
  willBeginBatch: function PU_willBeginBatch() {},
  willEndBatch: function PU_willEndBatch() {},
  didEndBatch: function PU_didEndBatch() {},
  willMerge: function PU_willMerge() {},
  didMerge: function PU_didMerge() {},


  /**
   * Determines if a node is read only (children cannot be inserted, sometimes
   * they cannot be removed depending on the circumstance)
   * @param   aNode
   *          A result node
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PU_nodeIsReadOnly(aNode) {
    let itemId = aNode.itemId;
    if (itemId != -1) {
      return this._readOnly.indexOf(itemId) != -1;
    }

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
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY],
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
   * Determines whether or not a node is a readonly folder.
   * @param   aNode
   *          The node to test.
   * @returns true if the node is a readonly folder.
  */
  isReadonlyFolder: function(aNode) {
    return this.nodeIsFolder(aNode) &&
           this._readOnly.indexOf(asQuery(aNode).folderItemId) != -1;
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
   * @return  A string serialization of the node
   */
  wrapNode: function PU_wrapNode(aNode, aType, aOverrideURI) {
    // when wrapping a node, we want all the items, even if the original
    // query options are excluding them.
    // this can happen when copying from the left hand pane of the bookmarks
    // organizer
    // @return [node, shouldClose]
    function convertNode(cNode) {
      if (PlacesUtils.nodeIsFolder(cNode) &&
          cNode.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT &&
          asQuery(cNode).queryOptions.excludeItems) {
        return [PlacesUtils.getFolderContents(cNode.itemId, false, true).root, true];
      }

      // If we didn't create our own query, do not alter the node's open state.
      return [cNode, false];
    }

    function gatherLivemarkUrl(aNode) {
      try {
        return PlacesUtils.annotations
                          .getItemAnnotation(aNode.itemId,
                                             PlacesUtils.LMANNO_SITEURI);
      } catch (ex) {
        return PlacesUtils.annotations
                          .getItemAnnotation(aNode.itemId,
                                             PlacesUtils.LMANNO_FEEDURI);
      }
    }

    function isLivemark(aNode) {
      return PlacesUtils.nodeIsFolder(aNode) &&
             PlacesUtils.annotations
                        .itemHasAnnotation(aNode.itemId,
                                           PlacesUtils.LMANNO_FEEDURI);
    }

    switch (aType) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER: {
        let writer = {
          value: "",
          write: function PU_wrapNode__write(aStr, aLen) {
            this.value += aStr;
          }
        };

        let [node, shouldClose] = convertNode(aNode);
        this._serializeNodeAsJSONToOutputStream(node, writer);
        if (shouldClose)
          node.containerOpen = false;

        return writer.value;
      }
      case this.TYPE_X_MOZ_URL: {
        function gatherDataUrl(bNode) {
          if (isLivemark(bNode)) {
            return gatherLivemarkUrl(bNode) + NEWLINE + bNode.title;
          }

          if (PlacesUtils.nodeIsURI(bNode))
            return (aOverrideURI || bNode.uri) + NEWLINE + bNode.title;
          // ignore containers and separators - items without valid URIs
          return "";
        }

        let [node, shouldClose] = convertNode(aNode);
        let dataUrl = gatherDataUrl(node);
        if (shouldClose)
          node.containerOpen = false;

        return dataUrl;
      }
      case this.TYPE_HTML: {
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
          let escapedTitle = bNode.title ? htmlEscape(bNode.title) : "";

          if (isLivemark(bNode)) {
            return "<A HREF=\"" + gatherLivemarkUrl(bNode) + "\">" + escapedTitle + "</A>" + NEWLINE;
          }

          if (PlacesUtils.nodeIsContainer(bNode)) {
            asContainer(bNode);
            let wasOpen = bNode.containerOpen;
            if (!wasOpen)
              bNode.containerOpen = true;

            let childString = "<DL><DT>" + escapedTitle + "</DT>" + NEWLINE;
            let cc = bNode.childCount;
            for (let i = 0; i < cc; ++i)
              childString += "<DD>"
                             + NEWLINE
                             + gatherDataHtml(bNode.getChild(i))
                             + "</DD>"
                             + NEWLINE;
            bNode.containerOpen = wasOpen;
            return childString + "</DL>" + NEWLINE;
          }
          if (PlacesUtils.nodeIsURI(bNode))
            return "<A HREF=\"" + bNode.uri + "\">" + escapedTitle + "</A>" + NEWLINE;
          if (PlacesUtils.nodeIsSeparator(bNode))
            return "<HR>" + NEWLINE;
          return "";
        }

        let [node, shouldClose] = convertNode(aNode);
        let dataHtml = gatherDataHtml(node);
        if (shouldClose)
          node.containerOpen = false;

        return dataHtml;
      }
    }

    // Otherwise, we wrap as TYPE_UNICODE.
    function gatherDataText(bNode) {
      if (isLivemark(bNode)) {
        return gatherLivemarkUrl(bNode);
      }

      if (PlacesUtils.nodeIsContainer(bNode)) {
        asContainer(bNode);
        let wasOpen = bNode.containerOpen;
        if (!wasOpen)
          bNode.containerOpen = true;

        let childString = bNode.title + NEWLINE;
        let cc = bNode.childCount;
        for (let i = 0; i < cc; ++i) {
          let child = bNode.getChild(i);
          let suffix = i < (cc - 1) ? NEWLINE : "";
          childString += gatherDataText(child) + suffix;
        }
        bNode.containerOpen = wasOpen;
        return childString;
      }
      if (PlacesUtils.nodeIsURI(bNode))
        return (aOverrideURI || bNode.uri);
      if (PlacesUtils.nodeIsSeparator(bNode))
        return "--------------------";
      return "";
    }

    let [node, shouldClose] = convertNode(aNode);
    let dataText = gatherDataText(node);
    // Convert node could pass an open container node.
    if (shouldClose)
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
        nodes = JSON.parse("[" + blob + "]");
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
   *         name, flags, expires, value
   */
  getAnnotationsForURI: function PU_getAnnotationsForURI(aURI) {
    var annosvc = this.annotations;
    var annos = [], val = null;
    var annoNames = annosvc.getPageAnnotationNames(aURI);
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, storageType = {};
      annosvc.getPageAnnotationInfo(aURI, annoNames[i], flags, exp, storageType);
      val = annosvc.getPageAnnotation(aURI, annoNames[i]);
      annos.push({name: annoNames[i],
                  flags: flags.value,
                  expires: exp.value,
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
      var flags = {}, exp = {}, storageType = {};
      annosvc.getItemAnnotationInfo(aItemId, annoNames[i], flags, exp, storageType);
      val = annosvc.getItemAnnotation(aItemId, annoNames[i]);
      annos.push({name: annoNames[i],
                  flags: flags.value,
                  expires: exp.value,
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
   *        name, flags, expires.
   *        If the value for an annotation is not set it will be removed.
   */
  setAnnotationsForURI: function PU_setAnnotationsForURI(aURI, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
      if (anno.value === undefined || anno.value === null) {
        annosvc.removePageAnnotation(aURI, anno.name);
      }
      else {
        let flags = ("flags" in anno) ? anno.flags : 0;
        let expires = ("expires" in anno) ?
          anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
        annosvc.setPageAnnotation(aURI, anno.name, anno.value, flags, expires);
      }
    });
  },

  /**
   * Annotate an item with a batch of annotations.
   * @param aItemId
   *        The identifier of the item for which annotations are to be set
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires.
   *        If the value for an annotation is not set it will be removed.
   */
  setAnnotationsForItem: function PU_setAnnotationsForItem(aItemId, aAnnos) {
    var annosvc = this.annotations;

    aAnnos.forEach(function(anno) {
      if (anno.value === undefined || anno.value === null) {
        annosvc.removeItemAnnotation(aItemId, anno.name);
      }
      else {
        let flags = ("flags" in anno) ? anno.flags : 0;
        let expires = ("expires" in anno) ?
          anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
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
      annos.setItemAnnotation(aBookmarkId, this.POST_DATA_ANNO, aPostData, 
                              0, Ci.nsIAnnotationService.EXPIRE_NEVER);
    else if (annos.itemHasAnnotation(aBookmarkId, this.POST_DATA_ANNO))
      annos.removeItemAnnotation(aBookmarkId, this.POST_DATA_ANNO);
  },

  /**
   * Get the POST data associated with a bookmark, if any.
   * @param aBookmarkId
   * @returns string of POST data if set for aBookmarkId. null otherwise.
   */
  getPostDataForBookmark: function PU_getPostDataForBookmark(aBookmarkId) {
    const annos = this.annotations;
    if (annos.itemHasAnnotation(aBookmarkId, this.POST_DATA_ANNO))
      return annos.getItemAnnotation(aBookmarkId, this.POST_DATA_ANNO);

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
   * Get all bookmarks for a URL, excluding items under tags.
   */
  getBookmarksForURI:
  function PU_getBookmarksForURI(aURI) {
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI);

    // filter the ids list
    return bmkIds.filter(function(aID) {
      var parentId = this.bookmarks.getFolderIdForItem(aID);
      var grandparentId = this.bookmarks.getFolderIdForItem(parentId);
      // item under a tag container
      if (grandparentId == this.tagsFolderId)
        return false;
      return true;
    }, this);
  },

  /**
   * Get the most recently added/modified bookmark for a URL, excluding items
   * under tags.
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
      if (grandparentId != this.tagsFolderId)
        return itemId;
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
  hasChildURIs: function PU_hasChildURIs(aNode, aMultiple=false) {
    if (!this.nodeIsContainer(aNode))
      return false;

    let root = this.getContainerNodeWithOptions(aNode, false, true);
    let result = root.parentResult;
    let didSuppressNotifications = false;
    let wasOpen = root.containerOpen;
    if (!wasOpen) {
      didSuppressNotifications = result.suppressNotifications;
      if (!didSuppressNotifications)
        result.suppressNotifications = true;

      root.containerOpen = true;
    }

    let foundFirst = !aMultiple;
    let found = false;
    for (let i = 0; i < root.childCount && !found; i++) {
      let child = root.getChild(i);
      if (this.nodeIsURI(child)) {
        found = foundFirst;
        foundFirst = true;
      }
    }

    if (!wasOpen) {
      root.containerOpen = false;
      if (!didSuppressNotifications)
        result.suppressNotifications = false;
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
    let urls = [];
    if (!this.nodeIsContainer(aNode))
      return urls;

    let root = this.getContainerNodeWithOptions(aNode, false, true);
    let result = root.parentResult;
    let wasOpen = root.containerOpen;
    let didSuppressNotifications = false;
    if (!wasOpen) {
      didSuppressNotifications = result.suppressNotifications;
      if (!didSuppressNotifications)
        result.suppressNotifications = true;

      root.containerOpen = true;
    }

    for (let i = 0; i < root.childCount; ++i) {
      let child = root.getChild(i);
      if (this.nodeIsURI(child))
        urls.push({uri: child.uri, isBookmark: this.nodeIsBookmark(child)});
    }

    if (!wasOpen) {
      root.containerOpen = false;
      if (!didSuppressNotifications)
        result.suppressNotifications = false;
    }
    return urls;
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
   */
  _serializeNodeAsJSONToOutputStream: function (aNode, aStream) {
    function addGenericProperties(aPlacesNode, aJSNode) {
      aJSNode.title = aPlacesNode.title;
      aJSNode.id = aPlacesNode.itemId;
      if (aJSNode.id != -1) {
        var parent = aPlacesNode.parent;
        if (parent) {
          aJSNode.parent = parent.itemId;
          aJSNode.parentReadOnly = PlacesUtils.nodeIsReadOnly(parent);
        }
        var dateAdded = aPlacesNode.dateAdded;
        if (dateAdded)
          aJSNode.dateAdded = dateAdded;
        var lastModified = aPlacesNode.lastModified;
        if (lastModified)
          aJSNode.lastModified = lastModified;

        // XXX need a hasAnnos api
        var annos = [];
        try {
          annos = PlacesUtils.getAnnotationsForItem(aJSNode.id).filter(function(anno) {
            // XXX should whitelist this instead, w/ a pref for
            // backup/restore of non-whitelisted annos
            // XXX causes JSON encoding errors, so utf-8 encode
            //anno.value = unescape(encodeURIComponent(anno.value));
            if (anno.name == PlacesUtils.LMANNO_FEEDURI)
              aJSNode.livemark = 1;
            return true;
          });
        } catch(ex) {}
        if (annos.length != 0)
          aJSNode.annos = annos;
      }
      // XXXdietrich - store annos for non-bookmark items
    }

    function addURIProperties(aPlacesNode, aJSNode) {
      aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      aJSNode.uri = aPlacesNode.uri;
      if (aJSNode.id && aJSNode.id != -1) {
        // harvest bookmark-specific properties
        var keyword = PlacesUtils.bookmarks.getKeywordForBookmark(aJSNode.id);
        if (keyword)
          aJSNode.keyword = keyword;
      }

      if (aPlacesNode.tags)
        aJSNode.tags = aPlacesNode.tags;

      // last character-set
      var uri = PlacesUtils._uri(aPlacesNode.uri);
      try {
        var lastCharset = PlacesUtils.annotations.getPageAnnotation(
                            uri, PlacesUtils.CHARSET_ANNO);
        aJSNode.charset = lastCharset;
      } catch (e) {}
    }

    function addSeparatorProperties(aPlacesNode, aJSNode) {
      aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
    }

    function addContainerProperties(aPlacesNode, aJSNode) {
      var concreteId = PlacesUtils.getConcreteItemId(aPlacesNode);
      if (concreteId != -1) {
        // This is a bookmark or a tag container.
        if (PlacesUtils.nodeIsQuery(aPlacesNode) ||
            concreteId != aPlacesNode.itemId) {
          aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
          aJSNode.uri = aPlacesNode.uri;
          // folder shortcut
          aJSNode.concreteId = concreteId;
        }
        else { // Bookmark folder or a shortcut we should convert to folder.
          aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;

          // Mark root folders.
          if (aJSNode.id == PlacesUtils.placesRootId)
            aJSNode.root = "placesRoot";
          else if (aJSNode.id == PlacesUtils.bookmarksMenuFolderId)
            aJSNode.root = "bookmarksMenuFolder";
          else if (aJSNode.id == PlacesUtils.tagsFolderId)
            aJSNode.root = "tagsFolder";
          else if (aJSNode.id == PlacesUtils.unfiledBookmarksFolderId)
            aJSNode.root = "unfiledBookmarksFolder";
          else if (aJSNode.id == PlacesUtils.toolbarFolderId)
            aJSNode.root = "toolbarFolder";
        }
      }
      else {
        // This is a grouped container query, generated on the fly.
        aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
        aJSNode.uri = aPlacesNode.uri;
      }
    }

    function appendConvertedComplexNode(aNode, aSourceNode, aArray) {
      var repr = {};

      for (let [name, value] in Iterator(aNode))
        repr[name] = value;

      // write child nodes
      var children = repr.children = [];
      if (!aNode.livemark) {
        asContainer(aSourceNode);
        var wasOpen = aSourceNode.containerOpen;
        if (!wasOpen)
          aSourceNode.containerOpen = true;
        var cc = aSourceNode.childCount;
        for (var i = 0; i < cc; ++i) {
          var childNode = aSourceNode.getChild(i);
          appendConvertedNode(aSourceNode.getChild(i), i, children);
        }
        if (!wasOpen)
          aSourceNode.containerOpen = false;
      }

      aArray.push(repr);
      return true;
    }

    function appendConvertedNode(bNode, aIndex, aArray) {
      var node = {};

      // set index in order received
      // XXX handy shortcut, but are there cases where we don't want
      // to export using the sorting provided by the query?
      if (aIndex)
        node.index = aIndex;

      addGenericProperties(bNode, node);

      var parent = bNode.parent;
      var grandParent = parent ? parent.parent : null;
      if (grandParent)
        node.grandParentId = grandParent.itemId;

      if (PlacesUtils.nodeIsURI(bNode)) {
        // Tag root accept only folder nodes
        if (parent && parent.itemId == PlacesUtils.tagsFolderId)
          return false;

        // Check for url validity, since we can't halt while writing a backup.
        // This will throw if we try to serialize an invalid url and it does
        // not make sense saving a wrong or corrupt uri node.
        try {
          PlacesUtils._uri(bNode.uri);
        } catch (ex) {
          return false;
        }

        addURIProperties(bNode, node);
      }
      else if (PlacesUtils.nodeIsContainer(bNode)) {
        // Tag containers accept only uri nodes
        if (grandParent && grandParent.itemId == PlacesUtils.tagsFolderId)
          return false;

        addContainerProperties(bNode, node);
      }
      else if (PlacesUtils.nodeIsSeparator(bNode)) {
        // Tag root accept only folder nodes
        // Tag containers accept only uri nodes
        if ((parent && parent.itemId == PlacesUtils.tagsFolderId) ||
            (grandParent && grandParent.itemId == PlacesUtils.tagsFolderId))
          return false;

        addSeparatorProperties(bNode, node);
      }

      if (!node.feedURI && node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER)
        return appendConvertedComplexNode(node, bNode, aArray);

      aArray.push(node);
      return true;
    }

    // serialize to stream
    var array = [];
    if (appendConvertedNode(aNode, null, array)) {
      var json = JSON.stringify(array[0]);
      aStream.write(json, json.length);
    }
    else {
      throw Cr.NS_ERROR_UNEXPECTED;
    }
  },

  /**
   * Gets the shared Sqlite.jsm readonly connection to the Places database.
   * This is intended to be used mostly internally, and by other Places modules.
   * Outside the Places component, it should be used only as a last resort.
   * Keep in mind the Places DB schema is by no means frozen or even stable.
   * Your custom queries can - and will - break overtime.
   */
  promiseDBConnection: () => gAsyncDBConnPromised,

  /**
   * Given a uri returns list of itemIds associated to it.
   *
   * @param aURI
   *        nsIURI or spec of the page.
   * @param aCallback
   *        Function to be called when done.
   *        The function will receive an array of itemIds associated to aURI and
   *        aURI itself.
   *
   * @return A object with a .cancel() method allowing to cancel the request.
   *
   * @note Children of live bookmarks folders are excluded. The callback function is
   *       not invoked if the request is cancelled or hits an error.
   */
  asyncGetBookmarkIds: function PU_asyncGetBookmarkIds(aURI, aCallback)
  {
    let abort = false;
    let itemIds = [];
    Task.spawn(function* () {
      let conn = yield this.promiseDBConnection();
      const QUERY_STR = "SELECT b.id FROM moz_bookmarks b " +
                        "JOIN moz_places h on h.id = b.fk " +
                        "WHERE h.url = :url";
      let spec = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
      yield conn.executeCached(QUERY_STR, { url: spec }, aRow => {
        if (abort)
          throw StopIteration;
        itemIds.push(aRow.getResultByIndex(0));  
      });
      if (!abort)
        aCallback(itemIds, aURI);
    }.bind(this)).then(null, Cu.reportError);
    return { cancel: () => { abort = true; } };
  },

  /**
   * Lazily adds a bookmarks observer, waiting for the bookmarks service to be
   * alive before registering the observer.  This is especially useful in the
   * startup path, to avoid initializing the service just to add an observer.
   *
   * @param aObserver
   *        Object implementing nsINavBookmarkObserver
   * @param [optional]aWeakOwner
   *        Whether to use weak ownership.
   *
   * @note Correct functionality of lazy observers relies on the fact Places
   *       notifies categories before real observers, and uses
   *       PlacesCategoriesStarter component to kick-off the registration.
   */
  _bookmarksServiceReady: false,
  _bookmarksServiceObserversQueue: [],
  addLazyBookmarkObserver:
  function PU_addLazyBookmarkObserver(aObserver, aWeakOwner) {
    if (this._bookmarksServiceReady) {
      this.bookmarks.addObserver(aObserver, aWeakOwner === true);
      return;
    }
    this._bookmarksServiceObserversQueue.push({ observer: aObserver,
                                                weak: aWeakOwner === true });
  },

  /**
   * Removes a bookmarks observer added through addLazyBookmarkObserver.
   *
   * @param aObserver
   *        Object implementing nsINavBookmarkObserver
   */
  removeLazyBookmarkObserver:
  function PU_removeLazyBookmarkObserver(aObserver) {
    if (this._bookmarksServiceReady) {
      this.bookmarks.removeObserver(aObserver);
      return;
    }
    let index = -1;
    for (let i = 0;
         i < this._bookmarksServiceObserversQueue.length && index == -1; i++) {
      if (this._bookmarksServiceObserversQueue[i].observer === aObserver)
        index = i;
    }
    if (index != -1) {
      this._bookmarksServiceObserversQueue.splice(index, 1);
    }
  },

  /**
   * Sets the character-set for a URI.
   *
   * @param aURI nsIURI
   * @param aCharset character-set value.
   * @return {Promise}
   */
  setCharsetForURI: function PU_setCharsetForURI(aURI, aCharset) {
    let deferred = Promise.defer();

    // Delaying to catch issues with asynchronous behavior while waiting
    // to implement asynchronous annotations in bug 699844.
    Services.tm.mainThread.dispatch(function() {
      if (aCharset && aCharset.length > 0) {
        PlacesUtils.annotations.setPageAnnotation(
          aURI, PlacesUtils.CHARSET_ANNO, aCharset, 0,
          Ci.nsIAnnotationService.EXPIRE_NEVER);
      } else {
        PlacesUtils.annotations.removePageAnnotation(
          aURI, PlacesUtils.CHARSET_ANNO);
      }
      deferred.resolve();
    }, Ci.nsIThread.DISPATCH_NORMAL);

    return deferred.promise;
  },

  /**
   * Gets the last saved character-set for a URI.
   *
   * @param aURI nsIURI
   * @return {Promise}
   * @resolve a character-set or null.
   */
  getCharsetForURI: function PU_getCharsetForURI(aURI) {
    let deferred = Promise.defer();

    Services.tm.mainThread.dispatch(function() {
      let charset = null;

      try {
        charset = PlacesUtils.annotations.getPageAnnotation(aURI,
                                                            PlacesUtils.CHARSET_ANNO);
      } catch (ex) { }

      deferred.resolve(charset);
    }, Ci.nsIThread.DISPATCH_NORMAL);

    return deferred.promise;
  },

  /**
   * Promised wrapper for mozIAsyncHistory::updatePlaces for a single place.
   *
   * @param aPlaces
   *        a single mozIPlaceInfo object
   * @resolves {Promise}
   */
  promiseUpdatePlace: function PU_promiseUpdatePlaces(aPlace) {
    let deferred = Promise.defer();
    PlacesUtils.asyncHistory.updatePlaces(aPlace, {
      _placeInfo: null,
      handleResult: function handleResult(aPlaceInfo) {
        this._placeInfo = aPlaceInfo;
      },
      handleError: function handleError(aResultCode, aPlaceInfo) {
        deferred.reject(new Components.Exception("Error", aResultCode));
      },
      handleCompletion: function() {
        deferred.resolve(this._placeInfo);
      }
    });

    return deferred.promise;
  },

  /**
   * Promised wrapper for mozIAsyncHistory::getPlacesInfo for a single place.
   *
   * @param aPlaceIdentifier
   *        either an nsIURI or a GUID (@see getPlacesInfo)
   * @resolves to the place info object handed to handleResult.
   */
  promisePlaceInfo: function PU_promisePlaceInfo(aPlaceIdentifier) {
    let deferred = Promise.defer();
    PlacesUtils.asyncHistory.getPlacesInfo(aPlaceIdentifier, {
      _placeInfo: null,
      handleResult: function handleResult(aPlaceInfo) {
        this._placeInfo = aPlaceInfo;
      },
      handleError: function handleError(aResultCode, aPlaceInfo) {
        deferred.reject(new Components.Exception("Error", aResultCode));
      },
      handleCompletion: function() {
        deferred.resolve(this._placeInfo);
      }
    });

    return deferred.promise;
  },

  /**
   * Gets favicon data for a given page url.
   *
   * @param aPageUrl url of the page to look favicon for.
   * @resolves to an object representing a favicon entry, having the following
   *           properties: { uri, dataLen, data, mimeType }
   * @rejects JavaScript exception if the given url has no associated favicon.
   */
  promiseFaviconData: function (aPageUrl) {
    let deferred = Promise.defer();
    PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(aPageUrl),
      function (aURI, aDataLen, aData, aMimeType) {
        if (aURI) {
          deferred.resolve({ uri: aURI,
                             dataLen: aDataLen,
                             data: aData,
                             mimeType: aMimeType });
        } else {
          deferred.reject();
        }
      });
    return deferred.promise;
  },

  /**
   * Get the unique id for an item (a bookmark, a folder or a separator) given
   * its item id.
   *
   * @param aItemId
   *        an item id
   * @return {Promise}
   * @resolves to the GUID.
   * @rejects if aItemId is invalid.
   */
  promiseItemGUID: function (aItemId) GUIDHelper.getItemGUID(aItemId),

  /**
   * Get the item id for an item (a bookmark, a folder or a separator) given
   * its unique id.
   *
   * @param aGUID
   *        an item GUID
   * @retrun {Promise}
   * @resolves to the GUID.
   * @rejects if there's no item for the given GUID.
   */
  promiseItemId: function (aGUID) GUIDHelper.getItemId(aGUID),

  /**
   * Asynchronously retrieve a JS-object representation of a places bookmarks
   * item (a bookmark, a folder, or a separator) along with all of its
   * descendants.
   *
   * @param [optional] aItemGUID
   *        the (topmost) item to be queried.  If it's not passed, the places
   *        root is queried: that is, you get a representation of the entire
   *        bookmarks hierarchy.
   * @param [optional] aOptions
   *        Options for customizing the query behavior, in the form of a JS
   *        object with any of the following properties:
   *         - excludeItemsCallback: a function for excluding items, along with
   *           their descendants.  Given an item object (that has everything set
   *           apart its potential children data), it should return true if the
   *           item should be excluded.  Once an item is excluded, the function
   *           isn't called for any of its descendants.  This isn't called for
   *           the root item.
   *           WARNING: since the function may be called for each item, using
   *           this option can slow down the process significantly if the
   *           callback does anything that's not relatively trivial.  It is
   *           highly recommended to avoid any synchronous I/O or DB queries.
   *
   * @return {Promise}
   * @resolves to a JS object that represents either a single item or a
   * bookmarks tree.  Each node in the tree has the following properties set:
   *  - guid (string): the item's guid (same as aItemGUID for the top item).
   *  - [deprecated] id (number): the item's id.  Only use it if you must. It'll
   *    be removed once the switch to guids is complete.
   *  - type (number):  the item's type.  @see PlacesUtils.TYPE_X_*
   *  - title (string): the item's title. If it has no title, this property
   *    isn't set.
   *  - dateAdded (number, microseconds from the epoch): the date-added value of
   *    the item.
   *  - lastModified (number, microseconds from the epoch): the last-modified
   *    value of the item.
   *  - annos (see getAnnotationsForItem): the item's annotations.  This is not
   *    set if there are no annotations set for the item).
   *
   * The root object (i.e. the one for aItemGUID) also has the following
   * properties set:
   *  - parentGUID (string): the guid of the root's parent.  This isn't set if
   *    the root item is the places root.
   *  - itemsCount (number, not enumerable): the number of items, including the
   *    root item itself, which are represented in the resolved object.
   *
   * Bookmark items also have the following properties:
   *  - uri (string): the item's url.
   *  - tags (string): csv string of the bookmark's tags.
   *  - charset (string): the last known charset of the bookmark.
   *  - keyword (string): the bookmark's keyword (unset if none).
   *  - iconuri (string): the bookmark's favicon url.
   * The last four properties are not set at all if they're irrelevant (e.g.
   * |charset| is not set if no charset was previously set for the bookmark
   * url).
   *
   * Folders may also have the following properties:
   *  - children (array): the folder's children information, each of them
   *    having the same set of properties as above.
   *
   * @rejects if the query failed for any reason.
   * @note if aItemGUID points to a non-existent item, the returned promise is
   * resolved to null.
   */
  promiseBookmarksTree: Task.async(function* (aItemGUID = "", aOptions = {}) {
    let createItemInfoObject = (aRow, aIncludeParentGUID) => {
      let item = {};
      let copyProps = (...props) => {
        for (let prop of props) {
          let val = aRow.getResultByName(prop);
          if (val !== null)
            item[prop] = val;
        }
      };
      copyProps("id" ,"guid", "title", "index", "dateAdded", "lastModified");
      if (aIncludeParentGUID)
        copyProps("parentGUID");

      let type = aRow.getResultByName("type");
      if (type == Ci.nsINavBookmarksService.TYPE_BOOKMARK)
        copyProps("charset", "tags", "iconuri");

      // Add annotations.
      if (aRow.getResultByName("has_annos")) {
        try {
          item.annos = PlacesUtils.getAnnotationsForItem(item.id);
        } catch (e) {
          Cu.reportError("Unexpected error while reading annotations " + e);
        }
      }

      switch (type) {
        case Ci.nsINavBookmarksService.TYPE_BOOKMARK:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE;
          // If this throws due to an invalid url, the item will be skipped.
          item.uri = NetUtil.newURI(aRow.getResultByName("url")).spec;
          // Keywords are cached, so this should be decently fast.
          let keyword = PlacesUtils.bookmarks.getKeywordForBookmark(item.id);
          if (keyword)
            item.keyword = keyword;
          break;
        case Ci.nsINavBookmarksService.TYPE_FOLDER:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
          // Mark root folders.
          if (item.id == PlacesUtils.placesRootId)
            item.root = "placesRoot";
          else if (item.id == PlacesUtils.bookmarksMenuFolderId)
            item.root = "bookmarksMenuFolder";
          else if (item.id == PlacesUtils.unfiledBookmarksFolderId)
            item.root = "unfiledBookmarksFolder";
          else if (item.id == PlacesUtils.toolbarFolderId)
            item.root = "toolbarFolder";
          break;
        case Ci.nsINavBookmarksService.TYPE_SEPARATOR:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
          break;
        default:
          Cu.reportError("Unexpected bookmark type");
          break;
      }
      return item;
    };

    const QUERY_STR =
      "WITH RECURSIVE " +
      "descendants(fk, level, type, id, guid, parent, parentGUID, position, " +
      "            title, dateAdded, lastModified) AS (" +
      "  SELECT b1.fk, 0, b1.type, b1.id, b1.guid, b1.parent, " +
      "         (SELECT guid FROM moz_bookmarks WHERE id = b1.parent), " +
      "         b1.position, b1.title, b1.dateAdded, b1.lastModified " +
      "  FROM moz_bookmarks b1 WHERE b1.guid=:item_guid " +
      "  UNION ALL " +
      "  SELECT b2.fk, level + 1, b2.type, b2.id, b2.guid, b2.parent, " +
      "         descendants.guid, b2.position, b2.title, b2.dateAdded, " +
      "         b2.lastModified " +
      "  FROM moz_bookmarks b2 " +
      "  JOIN descendants ON b2.parent = descendants.id AND b2.id <> :tags_folder) " +
      "SELECT d.level, d.id, d.guid, d.parent, d.parentGUID, d.type, " +
      "       d.position AS [index], d.title, d.dateAdded, d.lastModified, " +
      "       h.url, f.url AS iconuri, " +
      "       (SELECT GROUP_CONCAT(t.title, ',') " +
      "        FROM moz_bookmarks b2 " +
      "        JOIN moz_bookmarks t ON t.id = +b2.parent AND t.parent = :tags_folder " +
      "        WHERE b2.fk = h.id " +
      "       ) AS tags, " +
      "       EXISTS (SELECT 1 FROM moz_items_annos " +
      "               WHERE item_id = d.id LIMIT 1) AS has_annos, " +
      "       (SELECT a.content FROM moz_annos a " +
      "        JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id " +
      "        WHERE place_id = h.id AND n.name = :charset_anno " +
      "       ) AS charset " +
      "FROM descendants d " +
      "LEFT JOIN moz_bookmarks b3 ON b3.id = d.parent " +
      "LEFT JOIN moz_places h ON h.id = d.fk " +
      "LEFT JOIN moz_favicons f ON f.id = h.favicon_id " +
      "ORDER BY d.level, d.parent, d.position";


    if (!aItemGUID)
      aItemGUID = yield this.promiseItemGUID(PlacesUtils.placesRootId);

    let hasExcludeItemsCallback =
      aOptions.hasOwnProperty("excludeItemsCallback");
    let excludedParents = new Set();
    let shouldExcludeItem = (aItem, aParentGUID) => {
      let exclude = excludedParents.has(aParentGUID) ||
                    aOptions.excludeItemsCallback(aItem);
      if (exclude) {
        if (aItem.type == this.TYPE_X_MOZ_PLACE_CONTAINER)
          excludedParents.add(aItem.guid);
      }
      return exclude;
    };

    let rootItem = null, rootItemCreationEx = null;
    let parentsMap = new Map();
    try {
      let conn = yield this.promiseDBConnection();
      yield conn.executeCached(QUERY_STR,
          { tags_folder: PlacesUtils.tagsFolderId,
            charset_anno: PlacesUtils.CHARSET_ANNO,
            item_guid: aItemGUID }, (aRow) => {
        let item;
        if (!rootItem) {
          // This is the first row.
          try {
            rootItem = item = createItemInfoObject(aRow, true);
          }
          catch(ex) {
            // If we couldn't figure out the root item, that is just as bad
            // as a failed query.  Bail out.
            rootItemCreationEx = ex;
            throw StopIteration;
          }

          Object.defineProperty(rootItem, "itemsCount",
                                { value: 1
                                , writable: true
                                , enumerable: false
                                , configurable: false });
        }
        else {
          // Our query guarantees that we always visit parents ahead of their
          // children.
          item = createItemInfoObject(aRow, false);
          let parentGUID = aRow.getResultByName("parentGUID");
          if (hasExcludeItemsCallback && shouldExcludeItem(item, parentGUID))
            return;

          let parentItem = parentsMap.get(parentGUID);
          if ("children" in parentItem)
            parentItem.children.push(item);
          else
            parentItem.children = [item];

          rootItem.itemsCount++;
        }

        if (item.type == this.TYPE_X_MOZ_PLACE_CONTAINER)
          parentsMap.set(item.guid, item);
      });
    } catch(e) {
      throw new Error("Unable to query the database " + e);
    }
    if (rootItemCreationEx) {
      throw new Error("Failed to fetch the data for the root item" +
                      rootItemCreationEx);
    }

    return rootItem;
  })
};

XPCOMUtils.defineLazyGetter(PlacesUtils, "history", function() {
  return Cc["@mozilla.org/browser/nav-history-service;1"]
           .getService(Ci.nsINavHistoryService)
           .QueryInterface(Ci.nsIBrowserHistory)
           .QueryInterface(Ci.nsPIPlacesDatabase);
});

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "asyncHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

XPCOMUtils.defineLazyGetter(PlacesUtils, "bhistory", function() {
  return PlacesUtils.history;
});

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "favicons",
                                   "@mozilla.org/browser/favicon-service;1",
                                   "mozIAsyncFavicons");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "bookmarks",
                                   "@mozilla.org/browser/nav-bookmarks-service;1",
                                   "nsINavBookmarksService");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "annotations",
                                   "@mozilla.org/browser/annotation-service;1",
                                   "nsIAnnotationService");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "tagging",
                                   "@mozilla.org/browser/tagging-service;1",
                                   "nsITaggingService");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "livemarks",
                                   "@mozilla.org/browser/livemark-service;2",
                                   "mozIAsyncLivemarks");

XPCOMUtils.defineLazyGetter(PlacesUtils, "transactionManager", function() {
  let tm = Cc["@mozilla.org/transactionmanager;1"].
           createInstance(Ci.nsITransactionManager);
  tm.AddListener(PlacesUtils);
  this.registerShutdownFunction(function () {
    // Clear all references to local transactions in the transaction manager,
    // this prevents from leaking it.
    this.transactionManager.RemoveListener(this);
    this.transactionManager.clear();
  });

  // Bug 750269
  // The transaction manager keeps strong references to transactions, and by
  // that, also to the global for each transaction.  A transaction, however,
  // could be either the transaction itself (for which the global is this
  // module) or some js-proxy in another global, usually a window.  The later
  // would leak because the transaction lifetime (in the manager's stacks)
  // is independent of the global from which doTransaction was called.
  // To avoid such a leak, we hide the native doTransaction from callers,
  // and let each doTransaction call go through this module.
  // Doing so ensures that, as long as the transaction is any of the
  // PlacesXXXTransaction objects declared in this module, the object
  // referenced by the transaction manager has the module itself as global.
  return Object.create(tm, {
    "doTransaction": {
      value: function(aTransaction) {
        tm.doTransaction(aTransaction);
      }
    }
  });
});

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  const PLACES_STRING_BUNDLE_URI = "chrome://places/locale/places.properties";
  return Cc["@mozilla.org/intl/stringbundle;1"].
         getService(Ci.nsIStringBundleService).
         createBundle(PLACES_STRING_BUNDLE_URI);
});

XPCOMUtils.defineLazyGetter(this, "gAsyncDBConnPromised", () => {
  let connPromised = Sqlite.cloneStorageConnection({
    connection: PlacesUtils.history.DBConnection,
    readOnly:   true });
  connPromised.then(conn => {
    try {
      Sqlite.shutdown.addBlocker("Places DB readonly connection closing",
                                 conn.close.bind(conn));
    }
    catch(ex) {
      // It's too late to block shutdown, just close the connection.
      return conn.close();
    }
    return Promise.resolve();
  }).then(null, Cu.reportError);
  return connPromised;
});

// Sometime soon, likely as part of the transition to mozIAsyncBookmarks,
// itemIds will be deprecated in favour of GUIDs, which play much better
// with multiple undo/redo operations.  Because these GUIDs are already stored,
// and because we don't want to revise the transactions API once more when this
// happens, transactions are set to work with GUIDs exclusively, in the sense
// that they may never expose itemIds, nor do they accept them as input.
// More importantly, transactions which add or remove items guarantee to
// restore the guids on undo/redo, so that the following transactions that may
// done or undo can assume the items they're interested in are stil accessible
// through the same GUID.
// The current bookmarks API, however, doesn't expose the necessary means for
// working with GUIDs.  So, until it does, this helper object accesses the
// Places database directly in order to switch between GUIDs and itemIds, and
// "restore" GUIDs on items re-created items.
let GUIDHelper = {
  // Cache for guid<->itemId paris.
  GUIDsForIds: new Map(),
  idsForGUIDs: new Map(),

  getItemId: Task.async(function* (aGUID) {
    let cached = this.idsForGUIDs.get(aGUID);
    if (cached !== undefined)
      return cached;

    let conn = yield PlacesUtils.promiseDBConnection();
    let rows = yield conn.executeCached(
      "SELECT b.id, b.guid from moz_bookmarks b WHERE b.guid = :guid LIMIT 1",
      { guid: aGUID });
    if (rows.length == 0)
      throw new Error("no item found for the given guid");

    this.ensureObservingRemovedItems();
    let itemId = rows[0].getResultByName("id");
    this.idsForGUIDs.set(aGUID, itemId);
    return itemId;
  }),

  getItemGUID: Task.async(function* (aItemId) {
    let cached = this.GUIDsForIds.get(aItemId);
    if (cached !== undefined)
      return cached;

    let conn = yield PlacesUtils.promiseDBConnection();
    let rows = yield conn.executeCached(
      "SELECT b.id, b.guid from moz_bookmarks b WHERE b.id = :id LIMIT 1",
      { id: aItemId });
    if (rows.length == 0)
      throw new Error("no item found for the given itemId");

    this.ensureObservingRemovedItems();
    let guid = rows[0].getResultByName("guid");
    this.GUIDsForIds.set(aItemId, guid);
    return guid;
  }),

  ensureObservingRemovedItems: function () {
    if (!("observer" in this)) {
      /**
       * This observers serves two purposes:
       * (1) Invalidate cached id<->guid paris on when items are removed.
       * (2) Cache GUIDs given us free of charge by onItemAdded/onItemRemoved.
      *      So, for exmaple, when the NewBookmark needs the new GUID, we already
      *      have it cached.
      */
      this.observer = {
        onItemAdded: (aItemId, aParentId, aIndex, aItemType, aURI, aTitle,
                      aDateAdded, aGUID, aParentGUID) => {
          this.GUIDsForIds.set(aItemId, aGUID);
          this.GUIDsForIds.set(aParentId, aParentGUID);
        },
        onItemRemoved:
        (aItemId, aParentId, aIndex, aItemTyep, aURI, aGUID, aParentGUID) => {
          this.GUIDsForIds.delete(aItemId);
          this.idsForGUIDs.delete(aGUID);
          this.GUIDsForIds.set(aParentId, aParentGUID);
        },

        QueryInterface: XPCOMUtils.generateQI(Ci.nsINavBookmarkObserver),
        __noSuchMethod__: () => {}, // Catch all all onItem* methods.
      };
      PlacesUtils.bookmarks.addObserver(this.observer, false);
      PlacesUtils.registerShutdownFunction(() => {
        PlacesUtils.bookmarks.removeObserver(this.observer);
      });
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Transactions handlers.

/**
 * Updates commands in the undo group of the active window commands.
 * Inactive windows commands will be updated on focus.
 */
function updateCommandsOnActiveWindow()
{
  let win = Services.focus.activeWindow;
  if (win && win instanceof Ci.nsIDOMWindow) {
    // Updating "undo" will cause a group update including "redo".
    win.updateCommands("undo");
  }
}


/**
 * Used to cache bookmark information in transactions.
 *
 * @note To avoid leaks any non-primitive property should be copied.
 * @note Used internally, DO NOT EXPORT.
 */
function TransactionItemCache()
{
}

TransactionItemCache.prototype = {
  set id(v)
    this._id = (parseInt(v) > 0 ? v : null),
  get id()
    this._id || -1,
  set parentId(v)
    this._parentId = (parseInt(v) > 0 ? v : null),
  get parentId()
    this._parentId || -1,
  keyword: null,
  title: null,
  dateAdded: null,
  lastModified: null,
  postData: null,
  itemType: null,
  set uri(v)
    this._uri = (v instanceof Ci.nsIURI ? v.clone() : null),
  get uri()
    this._uri || null,
  set feedURI(v)
    this._feedURI = (v instanceof Ci.nsIURI ? v.clone() : null),
  get feedURI()
    this._feedURI || null,
  set siteURI(v)
    this._siteURI = (v instanceof Ci.nsIURI ? v.clone() : null),
  get siteURI()
    this._siteURI || null,
  set index(v)
    this._index = (parseInt(v) >= 0 ? v : null),
  // Index can be 0.
  get index()
    this._index != null ? this._index : PlacesUtils.bookmarks.DEFAULT_INDEX,
  set annotations(v)
    this._annotations = Array.isArray(v) ? Cu.cloneInto(v, {}) : null,
  get annotations()
    this._annotations || null,
  set tags(v)
    this._tags = (v && Array.isArray(v) ? Array.slice(v) : null),
  get tags()
    this._tags || null,
};


/**
 * Base transaction implementation.
 *
 * @note used internally, DO NOT EXPORT.
 */
function BaseTransaction()
{
}

BaseTransaction.prototype = {
  name: null,
  set childTransactions(v)
    this._childTransactions = (Array.isArray(v) ? Array.slice(v) : null),
  get childTransactions()
    this._childTransactions || null,
  doTransaction: function BTXN_doTransaction() {},
  redoTransaction: function BTXN_redoTransaction() this.doTransaction(),
  undoTransaction: function BTXN_undoTransaction() {},
  merge: function BTXN_merge() false,
  get isTransient() false,
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsITransaction
  ]),
};


/**
 * Transaction for performing several Places Transactions in a single batch. 
 * 
 * @param aName
 *        title of the aggregate transactions
 * @param aTransactions
 *        an array of transactions to perform
 *
 * @return nsITransaction object
 */
this.PlacesAggregatedTransaction =
 function PlacesAggregatedTransaction(aName, aTransactions)
{
  // Copy the transactions array to decouple it from its prototype, which
  // otherwise keeps alive its associated global object.
  this.childTransactions = aTransactions;
  this.name = aName;
  this.item = new TransactionItemCache();

  // Check child transactions number.  We will batch if we have more than
  // MIN_TRANSACTIONS_FOR_BATCH total number of transactions.
  let countTransactions = function(aTransactions, aTxnCount)
  {
    for (let i = 0;
         i < aTransactions.length && aTxnCount < MIN_TRANSACTIONS_FOR_BATCH;
         ++i, ++aTxnCount) {
      let txn = aTransactions[i];
      if (txn.childTransactions && txn.childTransactions.length > 0)
        aTxnCount = countTransactions(txn.childTransactions, aTxnCount);
    }
    return aTxnCount;
  }

  let txnCount = countTransactions(this.childTransactions, 0);
  this._useBatch = txnCount >= MIN_TRANSACTIONS_FOR_BATCH;
}

PlacesAggregatedTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function ATXN_doTransaction()
  {
    this._isUndo = false;
    if (this._useBatch)
      PlacesUtils.bookmarks.runInBatchMode(this, null);
    else
      this.runBatched(false);
  },

  undoTransaction: function ATXN_undoTransaction()
  {
    this._isUndo = true;
    if (this._useBatch)
      PlacesUtils.bookmarks.runInBatchMode(this, null);
    else
      this.runBatched(true);
  },

  runBatched: function ATXN_runBatched()
  {
    // Use a copy of the transactions array, so we won't reverse the original
    // one on undoing.
    let transactions = this.childTransactions.slice(0);
    if (this._isUndo)
      transactions.reverse();
    for (let i = 0; i < transactions.length; ++i) {
      let txn = transactions[i];
      if (this.item.parentId != -1)
        txn.item.parentId = this.item.parentId;
      if (this._isUndo)
        txn.undoTransaction();
      else
        txn.doTransaction();
    }
  }
};


/**
 * Transaction for creating a new folder.
 *
 * @param aTitle
 *        the title for the new folder
 * @param aParentId
 *        the id of the parent folder in which the new folder should be added
 * @param [optional] aIndex
 *        the index of the item in aParentId
 * @param [optional] aAnnotations
 *        array of annotations to set for the new folder
 * @param [optional] aChildTransactions
 *        array of transactions for items to be created in the new folder
 *
 * @return nsITransaction object
 */
this.PlacesCreateFolderTransaction =
 function PlacesCreateFolderTransaction(aTitle, aParentId, aIndex, aAnnotations,
                                        aChildTransactions)
{
  this.item = new TransactionItemCache();
  this.item.title = aTitle;
  this.item.parentId = aParentId;
  this.item.index = aIndex;
  this.item.annotations = aAnnotations;
  this.childTransactions = aChildTransactions;
}

PlacesCreateFolderTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CFTXN_doTransaction()
  { 
    this.item.id = PlacesUtils.bookmarks.createFolder(this.item.parentId,
                                                      this.item.title,
                                                      this.item.index);
    if (this.item.annotations && this.item.annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this.item.id, this.item.annotations);

    if (this.childTransactions && this.childTransactions.length > 0) {
      // Set the new parent id into child transactions.
      for (let i = 0; i < this.childTransactions.length; ++i) {
        this.childTransactions[i].item.parentId = this.item.id;
      }

      let txn = new PlacesAggregatedTransaction("Create folder childTxn",
                                                this.childTransactions);
      txn.doTransaction();
    }
  },

  undoTransaction: function CFTXN_undoTransaction()
  {
    if (this.childTransactions && this.childTransactions.length > 0) {
      let txn = new PlacesAggregatedTransaction("Create folder childTxn",
                                                this.childTransactions);
      txn.undoTransaction();
    }

    // Remove item only after all child transactions have been reverted.
    PlacesUtils.bookmarks.removeItem(this.item.id);
  }
};


/**
 * Transaction for creating a new bookmark.
 *
 * @param aURI
 *        the nsIURI of the new bookmark
 * @param aParentId
 *        the id of the folder in which the bookmark should be added.
 * @param [optional] aIndex
 *        the index of the item in aParentId
 * @param [optional] aTitle
 *        the title of the new bookmark
 * @param [optional] aKeyword
 *        the keyword for the new bookmark
 * @param [optional] aAnnotations
 *        array of annotations to set for the new bookmark
 * @param [optional] aChildTransactions
 *        child transactions to commit after creating the bookmark. Prefer
 *        using any of the arguments above if possible. In general, a child
 *        transations should be used only if the change it does has to be
 *        reverted manually when removing the bookmark item.
 *        a child transaction must support setting its bookmark-item
 *        identifier via an "id" js setter.
 *
 * @return nsITransaction object
 */
this.PlacesCreateBookmarkTransaction =
 function PlacesCreateBookmarkTransaction(aURI, aParentId, aIndex, aTitle,
                                          aKeyword, aAnnotations,
                                          aChildTransactions)
{
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.item.parentId = aParentId;
  this.item.index = aIndex;
  this.item.title = aTitle;
  this.item.keyword = aKeyword;
  this.item.annotations = aAnnotations;
  this.childTransactions = aChildTransactions;
}

PlacesCreateBookmarkTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CITXN_doTransaction()
  {
    this.item.id = PlacesUtils.bookmarks.insertBookmark(this.item.parentId,
                                                        this.item.uri,
                                                        this.item.index,
                                                        this.item.title);
    if (this.item.keyword) {
      PlacesUtils.bookmarks.setKeywordForBookmark(this.item.id,
                                                  this.item.keyword);
    }
    if (this.item.annotations && this.item.annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this.item.id, this.item.annotations);
 
    if (this.childTransactions && this.childTransactions.length > 0) {
      // Set the new item id into child transactions.
      for (let i = 0; i < this.childTransactions.length; ++i) {
        this.childTransactions[i].item.id = this.item.id;
      }
      let txn = new PlacesAggregatedTransaction("Create item childTxn",
                                                this.childTransactions);
      txn.doTransaction();
    }
  },

  undoTransaction: function CITXN_undoTransaction()
  {
    if (this.childTransactions && this.childTransactions.length > 0) {
      // Undo transactions should always be done in reverse order.
      let txn = new PlacesAggregatedTransaction("Create item childTxn",
                                                this.childTransactions);
      txn.undoTransaction();
    }

    // Remove item only after all child transactions have been reverted.
    PlacesUtils.bookmarks.removeItem(this.item.id);
  }
};


/**
 * Transaction for creating a new separator.
 *
 * @param aParentId
 *        the id of the folder in which the separator should be added
 * @param [optional] aIndex
 *        the index of the item in aParentId
 *
 * @return nsITransaction object
 */
this.PlacesCreateSeparatorTransaction =
 function PlacesCreateSeparatorTransaction(aParentId, aIndex)
{
  this.item = new TransactionItemCache();
  this.item.parentId = aParentId;
  this.item.index = aIndex;
}

PlacesCreateSeparatorTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CSTXN_doTransaction()
  {
    this.item.id =
      PlacesUtils.bookmarks.insertSeparator(this.item.parentId, this.item.index);
  },

  undoTransaction: function CSTXN_undoTransaction()
  {
    PlacesUtils.bookmarks.removeItem(this.item.id);
  }
};


/**
 * Transaction for creating a new livemark item.
 *
 * @see mozIAsyncLivemarks for documentation regarding the arguments.
 *
 * @param aFeedURI
 *        nsIURI of the feed
 * @param [optional] aSiteURI
 *        nsIURI of the page serving the feed
 * @param aTitle
 *        title for the livemark
 * @param aParentId
 *        the id of the folder in which the livemark should be added
 * @param [optional]  aIndex
 *        the index of the livemark in aParentId
 * @param [optional] aAnnotations
 *        array of annotations to set for the new livemark.
 *
 * @return nsITransaction object
 */
this.PlacesCreateLivemarkTransaction =
 function PlacesCreateLivemarkTransaction(aFeedURI, aSiteURI, aTitle, aParentId,
                                          aIndex, aAnnotations)
{
  this.item = new TransactionItemCache();
  this.item.feedURI = aFeedURI;
  this.item.siteURI = aSiteURI;
  this.item.title = aTitle;
  this.item.parentId = aParentId;
  this.item.index = aIndex;
  this.item.annotations = aAnnotations;
}

PlacesCreateLivemarkTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CLTXN_doTransaction()
  {
    PlacesUtils.livemarks.addLivemark(
      { title: this.item.title
      , feedURI: this.item.feedURI
      , parentId: this.item.parentId
      , index: this.item.index
      , siteURI: this.item.siteURI
      }).then(aLivemark => {
        this.item.id = aLivemark.id;
        if (this.item.annotations && this.item.annotations.length > 0) {
          PlacesUtils.setAnnotationsForItem(this.item.id,
                                            this.item.annotations);
        }
      }, Cu.reportError);
  },

  undoTransaction: function CLTXN_undoTransaction()
  {
    // The getLivemark callback may fail, but it is used just to serialize,
    // so it doesn't matter.
    PlacesUtils.livemarks.getLivemark({ id: this.item.id })
      .then(null, null).then( () => {
        PlacesUtils.bookmarks.removeItem(this.item.id);
      });
  }
};


/**
 * Transaction for removing a livemark item.
 *
 * @param aLivemarkId
 *        the identifier of the folder for the livemark.
 *
 * @return nsITransaction object
 * @note used internally by PlacesRemoveItemTransaction, DO NOT EXPORT.
 */
function PlacesRemoveLivemarkTransaction(aLivemarkId)
{
  this.item = new TransactionItemCache();
  this.item.id = aLivemarkId;
  this.item.title = PlacesUtils.bookmarks.getItemTitle(this.item.id);
  this.item.parentId = PlacesUtils.bookmarks.getFolderIdForItem(this.item.id);

  let annos = PlacesUtils.getAnnotationsForItem(this.item.id);
  // Exclude livemark service annotations, those will be recreated automatically
  let annosToExclude = [PlacesUtils.LMANNO_FEEDURI,
                        PlacesUtils.LMANNO_SITEURI];
  this.item.annotations = annos.filter(function(aValue, aIndex, aArray) {
      return annosToExclude.indexOf(aValue.name) == -1;
    });
  this.item.dateAdded = PlacesUtils.bookmarks.getItemDateAdded(this.item.id);
  this.item.lastModified =
    PlacesUtils.bookmarks.getItemLastModified(this.item.id);
}

PlacesRemoveLivemarkTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function RLTXN_doTransaction()
  {
    PlacesUtils.livemarks.getLivemark({ id: this.item.id })
      .then(aLivemark => {
        this.item.feedURI = aLivemark.feedURI;
        this.item.siteURI = aLivemark.siteURI;
        PlacesUtils.bookmarks.removeItem(this.item.id);
      }, Cu.reportError);
  },

  undoTransaction: function RLTXN_undoTransaction()
  {
    // Undo work must be serialized, otherwise won't be able to know the
    // feedURI and siteURI of the livemark.
    // The getLivemark callback is expected to receive a failure status but it
    // is used just to serialize, so doesn't matter.
    PlacesUtils.livemarks.getLivemark({ id: this.item.id })
      .then(null, () => {
        PlacesUtils.livemarks.addLivemark({ parentId: this.item.parentId
                                          , title: this.item.title
                                          , siteURI: this.item.siteURI
                                          , feedURI: this.item.feedURI
                                          , index: this.item.index
                                          , lastModified: this.item.lastModified
                                          }).then(
          aLivemark => {
            let itemId = aLivemark.id;
            PlacesUtils.bookmarks.setItemDateAdded(itemId, this.item.dateAdded);
            PlacesUtils.setAnnotationsForItem(itemId, this.item.annotations);
          }, Cu.reportError);
      });
  }
};


/**
 * Transaction for moving an Item.
 *
 * @param aItemId
 *        the id of the item to move
 * @param aNewParentId
 *        id of the new parent to move to
 * @param aNewIndex
 *        index of the new position to move to
 *
 * @return nsITransaction object
 */
this.PlacesMoveItemTransaction =
 function PlacesMoveItemTransaction(aItemId, aNewParentId, aNewIndex)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.item.parentId = PlacesUtils.bookmarks.getFolderIdForItem(this.item.id);
  this.new = new TransactionItemCache();
  this.new.parentId = aNewParentId;
  this.new.index = aNewIndex;
}

PlacesMoveItemTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function MITXN_doTransaction()
  {
    this.item.index = PlacesUtils.bookmarks.getItemIndex(this.item.id);
    PlacesUtils.bookmarks.moveItem(this.item.id,
                                   this.new.parentId, this.new.index);
    this._undoIndex = PlacesUtils.bookmarks.getItemIndex(this.item.id);
  },

  undoTransaction: function MITXN_undoTransaction()
  {
    // moving down in the same parent takes in count removal of the item
    // so to revert positions we must move to oldIndex + 1
    if (this.new.parentId == this.item.parentId &&
        this.item.index > this._undoIndex) {
      PlacesUtils.bookmarks.moveItem(this.item.id, this.item.parentId,
                                     this.item.index + 1);
    }
    else {
      PlacesUtils.bookmarks.moveItem(this.item.id, this.item.parentId,
                                     this.item.index);
    }
  }
};


/**
 * Transaction for removing an Item
 *
 * @param aItemId
 *        id of the item to remove
 *
 * @return nsITransaction object
 */
this.PlacesRemoveItemTransaction =
 function PlacesRemoveItemTransaction(aItemId)
{
  if (PlacesUtils.isRootItem(aItemId))
    throw Cr.NS_ERROR_INVALID_ARG;

  // if the item lives within a tag container, use the tagging transactions
  let parent = PlacesUtils.bookmarks.getFolderIdForItem(aItemId);
  let grandparent = PlacesUtils.bookmarks.getFolderIdForItem(parent);
  if (grandparent == PlacesUtils.tagsFolderId) {
    let uri = PlacesUtils.bookmarks.getBookmarkURI(aItemId);
    return new PlacesUntagURITransaction(uri, [parent]);
  }

  // if the item is a livemark container we will not save its children.
  if (PlacesUtils.annotations.itemHasAnnotation(aItemId,
                                                PlacesUtils.LMANNO_FEEDURI))
    return new PlacesRemoveLivemarkTransaction(aItemId);

  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.item.itemType = PlacesUtils.bookmarks.getItemType(this.item.id);
  if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
    this.childTransactions = this._getFolderContentsTransactions();
    // Remove this folder itself.
    let txn = PlacesUtils.bookmarks.getRemoveFolderTransaction(this.item.id);
    this.childTransactions.push(txn);
  }
  else if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
    this.item.uri = PlacesUtils.bookmarks.getBookmarkURI(this.item.id);
    this.item.keyword =
      PlacesUtils.bookmarks.getKeywordForBookmark(this.item.id);
  }

  if (this.item.itemType != Ci.nsINavBookmarksService.TYPE_SEPARATOR)
    this.item.title = PlacesUtils.bookmarks.getItemTitle(this.item.id);

  this.item.parentId = PlacesUtils.bookmarks.getFolderIdForItem(this.item.id);
  this.item.annotations = PlacesUtils.getAnnotationsForItem(this.item.id);
  this.item.dateAdded = PlacesUtils.bookmarks.getItemDateAdded(this.item.id);
  this.item.lastModified =
    PlacesUtils.bookmarks.getItemLastModified(this.item.id);
}

PlacesRemoveItemTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function RITXN_doTransaction()
  {
    this.item.index = PlacesUtils.bookmarks.getItemIndex(this.item.id);

    if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      let txn = new PlacesAggregatedTransaction("Remove item childTxn",
                                                this.childTransactions);
      txn.doTransaction();
    }
    else {
      // Before removing the bookmark, save its tags.
      let tags = this.item.uri ?
        PlacesUtils.tagging.getTagsForURI(this.item.uri) : null;

      PlacesUtils.bookmarks.removeItem(this.item.id);

      // If this was the last bookmark (excluding tag-items) for this url,
      // persist the tags.
      if (tags && PlacesUtils.getMostRecentBookmarkForURI(this.item.uri) == -1) {
        this.item.tags = tags;
      }
    }
  },

  undoTransaction: function RITXN_undoTransaction()
  {
    if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
      this.item.id = PlacesUtils.bookmarks.insertBookmark(this.item.parentId,
                                                          this.item.uri,
                                                          this.item.index,
                                                          this.item.title);
      if (this.item.tags && this.item.tags.length > 0)
        PlacesUtils.tagging.tagURI(this.item.uri, this.item.tags);
      if (this.item.keyword) {
        PlacesUtils.bookmarks.setKeywordForBookmark(this.item.id,
                                                    this.item.keyword);
      }
    }
    else if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      let txn = new PlacesAggregatedTransaction("Remove item childTxn",
                                                this.childTransactions);
      txn.undoTransaction();
    }
    else { // TYPE_SEPARATOR
      this.item.id = PlacesUtils.bookmarks.insertSeparator(this.item.parentId,
                                                            this.item.index);
    }

    if (this.item.annotations && this.item.annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this.item.id, this.item.annotations);

    PlacesUtils.bookmarks.setItemDateAdded(this.item.id, this.item.dateAdded);
    PlacesUtils.bookmarks.setItemLastModified(this.item.id,
                                              this.item.lastModified);
  },

  /**
  * Returns a flat, ordered list of transactions for a depth-first recreation
  * of items within this folder.
  */
  _getFolderContentsTransactions:
  function RITXN__getFolderContentsTransactions()
  {
    let transactions = [];
    let contents =
      PlacesUtils.getFolderContents(this.item.id, false, false).root;
    for (let i = 0; i < contents.childCount; ++i) {
      let txn = new PlacesRemoveItemTransaction(contents.getChild(i).itemId);
      transactions.push(txn);
    }
    contents.containerOpen = false;
    // Reverse transactions to preserve parent-child relationship.
    return transactions.reverse();
  }
};


/**
 * Transaction for editting a bookmark's title.
 *
 * @param aItemId
 *        id of the item to edit
 * @param aNewTitle
 *        new title for the item to edit
 *
 * @return nsITransaction object
 */
this.PlacesEditItemTitleTransaction =
 function PlacesEditItemTitleTransaction(aItemId, aNewTitle)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.title = aNewTitle;
}

PlacesEditItemTitleTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EITTXN_doTransaction()
  {
    this.item.title = PlacesUtils.bookmarks.getItemTitle(this.item.id);
    PlacesUtils.bookmarks.setItemTitle(this.item.id, this.new.title);
  },

  undoTransaction: function EITTXN_undoTransaction()
  {
    PlacesUtils.bookmarks.setItemTitle(this.item.id, this.item.title);
  }
};


/**
 * Transaction for editing a bookmark's uri.
 *
 * @param aItemId
 *        id of the bookmark to edit
 * @param aNewURI
 *        new uri for the bookmark
 *
 * @return nsITransaction object
 */
this.PlacesEditBookmarkURITransaction =
 function PlacesEditBookmarkURITransaction(aItemId, aNewURI) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.uri = aNewURI;
}

PlacesEditBookmarkURITransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EBUTXN_doTransaction()
  {
    this.item.uri = PlacesUtils.bookmarks.getBookmarkURI(this.item.id);
    PlacesUtils.bookmarks.changeBookmarkURI(this.item.id, this.new.uri);
    // move tags from old URI to new URI
    this.item.tags = PlacesUtils.tagging.getTagsForURI(this.item.uri);
    if (this.item.tags.length != 0) {
      // only untag the old URI if this is the only bookmark
      if (PlacesUtils.getBookmarksForURI(this.item.uri, {}).length == 0)
        PlacesUtils.tagging.untagURI(this.item.uri, this.item.tags);
      PlacesUtils.tagging.tagURI(this.new.uri, this.item.tags);
    }
  },

  undoTransaction: function EBUTXN_undoTransaction()
  {
    PlacesUtils.bookmarks.changeBookmarkURI(this.item.id, this.item.uri);
    // move tags from new URI to old URI 
    if (this.item.tags.length != 0) {
      // only untag the new URI if this is the only bookmark
      if (PlacesUtils.getBookmarksForURI(this.new.uri, {}).length == 0)
        PlacesUtils.tagging.untagURI(this.new.uri, this.item.tags);
      PlacesUtils.tagging.tagURI(this.item.uri, this.item.tags);
    }
  }
};


/**
 * Transaction for setting/unsetting an item annotation
 *
 * @param aItemId
 *        id of the item where to set annotation
 * @param aAnnotationObject
 *        Object representing an annotation, containing the following
 *        properties: name, flags, expires, value.
 *        If value is null the annotation will be removed
 *
 * @return nsITransaction object
 */
this.PlacesSetItemAnnotationTransaction =
 function PlacesSetItemAnnotationTransaction(aItemId, aAnnotationObject)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.annotations = [aAnnotationObject];
}

PlacesSetItemAnnotationTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SIATXN_doTransaction()
  {
    let annoName = this.new.annotations[0].name;
    if (PlacesUtils.annotations.itemHasAnnotation(this.item.id, annoName)) {
      // fill the old anno if it is set
      let flags = {}, expires = {}, type = {};
      PlacesUtils.annotations.getItemAnnotationInfo(this.item.id, annoName, flags,
                                                    expires, type);
      let value = PlacesUtils.annotations.getItemAnnotation(this.item.id,
                                                            annoName);
      this.item.annotations = [{ name: annoName,
                                type: type.value,
                                flags: flags.value,
                                value: value,
                                expires: expires.value }];
    }
    else {
      // create an empty old anno
      this.item.annotations = [{ name: annoName,
                                flags: 0,
                                value: null,
                                expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
    }

    PlacesUtils.setAnnotationsForItem(this.item.id, this.new.annotations);
  },

  undoTransaction: function SIATXN_undoTransaction()
  {
    PlacesUtils.setAnnotationsForItem(this.item.id, this.item.annotations);
  }
};


/**
 * Transaction for setting/unsetting a page annotation
 *
 * @param aURI
 *        URI of the page where to set annotation
 * @param aAnnotationObject
 *        Object representing an annotation, containing the following
 *        properties: name, flags, expires, value.
 *        If value is null the annotation will be removed
 *
 * @return nsITransaction object
 */
this.PlacesSetPageAnnotationTransaction =
 function PlacesSetPageAnnotationTransaction(aURI, aAnnotationObject)
{
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.new = new TransactionItemCache();
  this.new.annotations = [aAnnotationObject];
}

PlacesSetPageAnnotationTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SPATXN_doTransaction()
  {
    let annoName = this.new.annotations[0].name;
    if (PlacesUtils.annotations.pageHasAnnotation(this.item.uri, annoName)) {
      // fill the old anno if it is set
      let flags = {}, expires = {}, type = {};
      PlacesUtils.annotations.getPageAnnotationInfo(this.item.uri, annoName, flags,
                                                    expires, type);
      let value = PlacesUtils.annotations.getPageAnnotation(this.item.uri,
                                                            annoName);
      this.item.annotations = [{ name: annoName,
                                flags: flags.value,
                                value: value,
                                expires: expires.value }];
    }
    else {
      // create an empty old anno
      this.item.annotations = [{ name: annoName,
                                type: Ci.nsIAnnotationService.TYPE_STRING,
                                flags: 0,
                                value: null,
                                expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
    }

    PlacesUtils.setAnnotationsForURI(this.item.uri, this.new.annotations);
  },

  undoTransaction: function SPATXN_undoTransaction()
  {
    PlacesUtils.setAnnotationsForURI(this.item.uri, this.item.annotations);
  }
};


/**
 * Transaction for editing a bookmark's keyword.
 *
 * @param aItemId
 *        id of the bookmark to edit
 * @param aNewKeyword
 *        new keyword for the bookmark
 *
 * @return nsITransaction object
 */
this.PlacesEditBookmarkKeywordTransaction =
 function PlacesEditBookmarkKeywordTransaction(aItemId, aNewKeyword)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.keyword = aNewKeyword;
}

PlacesEditBookmarkKeywordTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EBKTXN_doTransaction()
  {
    this.item.keyword = PlacesUtils.bookmarks.getKeywordForBookmark(this.item.id);
    PlacesUtils.bookmarks.setKeywordForBookmark(this.item.id, this.new.keyword);
  },

  undoTransaction: function EBKTXN_undoTransaction()
  {
    PlacesUtils.bookmarks.setKeywordForBookmark(this.item.id, this.item.keyword);
  }
};


/**
 * Transaction for editing the post data associated with a bookmark.
 *
 * @param aItemId
 *        id of the bookmark to edit
 * @param aPostData
 *        post data
 *
 * @return nsITransaction object
 */
this.PlacesEditBookmarkPostDataTransaction =
 function PlacesEditBookmarkPostDataTransaction(aItemId, aPostData)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.postData = aPostData;
}

PlacesEditBookmarkPostDataTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EBPDTXN_doTransaction()
  {
    this.item.postData = PlacesUtils.getPostDataForBookmark(this.item.id);
    PlacesUtils.setPostDataForBookmark(this.item.id, this.new.postData);
  },

  undoTransaction: function EBPDTXN_undoTransaction()
  {
    PlacesUtils.setPostDataForBookmark(this.item.id, this.item.postData);
  }
};


/**
 * Transaction for editing an item's date added property.
 *
 * @param aItemId
 *        id of the item to edit
 * @param aNewDateAdded
 *        new date added for the item 
 *
 * @return nsITransaction object
 */
this.PlacesEditItemDateAddedTransaction =
 function PlacesEditItemDateAddedTransaction(aItemId, aNewDateAdded)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.dateAdded = aNewDateAdded;
}

PlacesEditItemDateAddedTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EIDATXN_doTransaction()
  {
    // Child transactions have the id set as parentId.
    if (this.item.id == -1 && this.item.parentId != -1)
      this.item.id = this.item.parentId;
    this.item.dateAdded =
      PlacesUtils.bookmarks.getItemDateAdded(this.item.id);
    PlacesUtils.bookmarks.setItemDateAdded(this.item.id, this.new.dateAdded);
  },

  undoTransaction: function EIDATXN_undoTransaction()
  {
    PlacesUtils.bookmarks.setItemDateAdded(this.item.id, this.item.dateAdded);
  }
};


/**
 * Transaction for editing an item's last modified time.
 *
 * @param aItemId
 *        id of the item to edit
 * @param aNewLastModified
 *        new last modified date for the item 
 *
 * @return nsITransaction object
 */
this.PlacesEditItemLastModifiedTransaction =
 function PlacesEditItemLastModifiedTransaction(aItemId, aNewLastModified)
{
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.lastModified = aNewLastModified;
}

PlacesEditItemLastModifiedTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction:
  function EILMTXN_doTransaction()
  {
    // Child transactions have the id set as parentId.
    if (this.item.id == -1 && this.item.parentId != -1)
      this.item.id = this.item.parentId;
    this.item.lastModified =
      PlacesUtils.bookmarks.getItemLastModified(this.item.id);
    PlacesUtils.bookmarks.setItemLastModified(this.item.id,
                                              this.new.lastModified);
  },

  undoTransaction:
  function EILMTXN_undoTransaction()
  {
    PlacesUtils.bookmarks.setItemLastModified(this.item.id,
                                              this.item.lastModified);
  }
};


/**
 * Transaction for sorting a folder by name
 *
 * @param aFolderId
 *        id of the folder to sort
 *
 * @return nsITransaction object
 */
this.PlacesSortFolderByNameTransaction =
 function PlacesSortFolderByNameTransaction(aFolderId)
{
  this.item = new TransactionItemCache();  
  this.item.id = aFolderId;
}

PlacesSortFolderByNameTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SFBNTXN_doTransaction()
  {
    this._oldOrder = [];

    let contents =
      PlacesUtils.getFolderContents(this.item.id, false, false).root;
    let count = contents.childCount;

    // sort between separators
    let newOrder = []; 
    let preSep = []; // temporary array for sorting each group of items
    let sortingMethod =
      function (a, b) {
        if (PlacesUtils.nodeIsContainer(a) && !PlacesUtils.nodeIsContainer(b))
          return -1;
        if (!PlacesUtils.nodeIsContainer(a) && PlacesUtils.nodeIsContainer(b))
          return 1;
        return a.title.localeCompare(b.title);
      };

    for (let i = 0; i < count; ++i) {
      let item = contents.getChild(i);
      this._oldOrder[item.itemId] = i;
      if (PlacesUtils.nodeIsSeparator(item)) {
        if (preSep.length > 0) {
          preSep.sort(sortingMethod);
          newOrder = newOrder.concat(preSep);
          preSep.splice(0, preSep.length);
        }
        newOrder.push(item);
      }
      else
        preSep.push(item);
    }
    contents.containerOpen = false;

    if (preSep.length > 0) {
      preSep.sort(sortingMethod);
      newOrder = newOrder.concat(preSep);
    }

    // set the nex indexes
    let callback = {
      runBatched: function() {
        for (let i = 0; i < newOrder.length; ++i) {
          PlacesUtils.bookmarks.setItemIndex(newOrder[i].itemId, i);
        }
      }
    };
    PlacesUtils.bookmarks.runInBatchMode(callback, null);
  },

  undoTransaction: function SFBNTXN_undoTransaction()
  {
    let callback = {
      _self: this,
      runBatched: function() {
        for (item in this._self._oldOrder)
          PlacesUtils.bookmarks.setItemIndex(item, this._self._oldOrder[item]);
      }
    };
    PlacesUtils.bookmarks.runInBatchMode(callback, null);
  }
};


/**
 * Transaction for tagging a URL with the given set of tags. Current tags set
 * for the URL persist. It's the caller's job to check whether or not aURI
 * was already tagged by any of the tags in aTags, undoing this tags
 * transaction removes them all from aURL!
 *
 * @param aURI
 *        the URL to tag.
 * @param aTags
 *        Array of tags to set for the given URL.
 */
this.PlacesTagURITransaction =
 function PlacesTagURITransaction(aURI, aTags)
{
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.item.tags = aTags;
}

PlacesTagURITransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function TUTXN_doTransaction()
  {
    if (PlacesUtils.getMostRecentBookmarkForURI(this.item.uri) == -1) {
      // There is no bookmark for this uri, but we only allow to tag bookmarks.
      // Force an unfiled bookmark first.
      this.item.id =
        PlacesUtils.bookmarks
                   .insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                   this.item.uri,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX,
                                   PlacesUtils.history.getPageTitle(this.item.uri));
    }
    PlacesUtils.tagging.tagURI(this.item.uri, this.item.tags);
  },

  undoTransaction: function TUTXN_undoTransaction()
  {
    if (this.item.id != -1) {
      PlacesUtils.bookmarks.removeItem(this.item.id);
      this.item.id = -1;
    }
    PlacesUtils.tagging.untagURI(this.item.uri, this.item.tags);
  }
};


/**
 * Transaction for removing tags from a URL. It's the caller's job to check
 * whether or not aURI isn't tagged by any of the tags in aTags, undoing this
 * tags transaction adds them all to aURL!
 *
 * @param aURI
 *        the URL to un-tag.
 * @param aTags
 *        Array of tags to unset. pass null to remove all tags from the given
 *        url.
 */
this.PlacesUntagURITransaction =
 function PlacesUntagURITransaction(aURI, aTags)
{
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  if (aTags) {
    // Within this transaction, we cannot rely on tags given by itemId
    // since the tag containers may be gone after we call untagURI.
    // Thus, we convert each tag given by its itemId to name.
    let tags = [];
    for (let i = 0; i < aTags.length; ++i) {
      if (typeof(aTags[i]) == "number")
        tags.push(PlacesUtils.bookmarks.getItemTitle(aTags[i]));
      else
        tags.push(aTags[i]);
    }
    this.item.tags = tags;
  }
}

PlacesUntagURITransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function UTUTXN_doTransaction()
  {
    // Filter tags existing on the bookmark, otherwise on undo we may try to
    // set nonexistent tags.
    let tags = PlacesUtils.tagging.getTagsForURI(this.item.uri);
    this.item.tags = this.item.tags.filter(function (aTag) {
      return tags.indexOf(aTag) != -1;
    });
    PlacesUtils.tagging.untagURI(this.item.uri, this.item.tags);
  },

  undoTransaction: function UTUTXN_undoTransaction()
  {
    PlacesUtils.tagging.tagURI(this.item.uri, this.item.tags);
  }
};
