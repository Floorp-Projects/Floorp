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

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Bookmarks",
                                  "resource://gre/modules/Bookmarks.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "History",
                                  "resource://gre/modules/History.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesSyncUtils",
                                  "resource://gre/modules/PlacesSyncUtils.jsm");

// The minimum amount of transactions before starting a batch. Usually we do
// do incremental updates, a batch will cause views to completely
// refresh instead.
const MIN_TRANSACTIONS_FOR_BATCH = 5;

// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where
// we really just want "\n". On other platforms, the transferable system
// converts "\r\n" to "\n".
const NEWLINE = AppConstants.platform == "macosx" ? "\n" : "\r\n";

// Timers resolution is not always good, it can have a 16ms precision on Win.
const TIMERS_RESOLUTION_SKEW_MS = 16;

function QI_node(aNode, aIID) {
  var result = null;
  try {
    result = aNode.QueryInterface(aIID);
  } catch (e) {
  }
  return result;
}
function asContainer(aNode) {
  return QI_node(aNode, Ci.nsINavHistoryContainerResultNode);
}
function asQuery(aNode) {
  return QI_node(aNode, Ci.nsINavHistoryQueryResultNode);
}

/**
 * Sends a bookmarks notification through the given observers.
 *
 * @param observers
 *        array of nsINavBookmarkObserver objects.
 * @param notification
 *        the notification name.
 * @param args
 *        array of arguments to pass to the notification.
 */
function notify(observers, notification, args) {
  for (let observer of observers) {
    try {
      observer[notification](...args);
    } catch (ex) {}
  }
}

/**
 * Sends a keyword change notification.
 *
 * @param url
 *        the url to notify about.
 * @param keyword
 *        The keyword to notify, or empty string if a keyword was removed.
 */
async function notifyKeywordChange(url, keyword, source) {
  // Notify bookmarks about the removal.
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url }, b => bookmarks.push(b));
  // We don't want to yield in the gIgnoreKeywordNotifications section.
  for (let bookmark of bookmarks) {
    bookmark.id = await PlacesUtils.promiseItemId(bookmark.guid);
    bookmark.parentId = await PlacesUtils.promiseItemId(bookmark.parentGuid);
  }
  let observers = PlacesUtils.bookmarks.getObservers();
  gIgnoreKeywordNotifications = true;
  for (let bookmark of bookmarks) {
    notify(observers, "onItemChanged", [ bookmark.id, "keyword", false,
                                         keyword,
                                         bookmark.lastModified * 1000,
                                         bookmark.type,
                                         bookmark.parentId,
                                         bookmark.guid, bookmark.parentGuid,
                                         "", source
                                       ]);
  }
  gIgnoreKeywordNotifications = false;
}

/**
 * Serializes the given node in JSON format.
 *
 * @param aNode
 *        An nsINavHistoryResultNode
 * @param aIsLivemark
 *        Whether the node represents a livemark.
 */
function serializeNode(aNode, aIsLivemark) {
  let data = {};

  data.title = aNode.title;
  data.id = aNode.itemId;
  data.livemark = aIsLivemark;

  let guid = aNode.bookmarkGuid;
  if (guid) {
    data.itemGuid = guid;
    if (aNode.parent)
      data.parent = aNode.parent.itemId;
    let grandParent = aNode.parent && aNode.parent.parent;
    if (grandParent)
      data.grandParentId = grandParent.itemId;

    data.dateAdded = aNode.dateAdded;
    data.lastModified = aNode.lastModified;

    let annos = PlacesUtils.getAnnotationsForItem(data.id);
    if (annos.length > 0)
      data.annos = annos;
  }

  if (PlacesUtils.nodeIsURI(aNode)) {
    // Check for url validity.
    NetUtil.newURI(aNode.uri);

    // Tag root accepts only folder nodes, not URIs.
    if (data.parent == PlacesUtils.tagsFolderId)
      throw new Error("Unexpected node type");

    data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
    data.uri = aNode.uri;

    if (aNode.tags)
      data.tags = aNode.tags;
  } else if (PlacesUtils.nodeIsContainer(aNode)) {
    // Tag containers accept only uri nodes.
    if (data.grandParentId == PlacesUtils.tagsFolderId)
      throw new Error("Unexpected node type");

    let concreteId = PlacesUtils.getConcreteItemId(aNode);
    if (concreteId != -1) {
      // This is a bookmark or a tag container.
      if (PlacesUtils.nodeIsQuery(aNode) || concreteId != aNode.itemId) {
        // This is a folder shortcut.
        data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
        data.uri = aNode.uri;
        data.concreteId = concreteId;
      } else {
        // This is a bookmark folder.
        data.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
      }
    } else {
      // This is a grouped container query, dynamically generated.
      data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      data.uri = aNode.uri;
    }
  } else if (PlacesUtils.nodeIsSeparator(aNode)) {
    // Tag containers don't accept separators.
    if (data.parent == PlacesUtils.tagsFolderId ||
        data.grandParentId == PlacesUtils.tagsFolderId)
      throw new Error("Unexpected node type");

    data.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
  }

  return JSON.stringify(data);
}

// Imposed to limit database size.
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;

/**
 * List of bookmark object validators, one per each known property.
 * Validators must throw if the property value is invalid and return a fixed up
 * version of the value, if needed.
 */
const BOOKMARK_VALIDATORS = Object.freeze({
  guid: simpleValidateFunc(v => PlacesUtils.isValidGuid(v)),
  parentGuid: simpleValidateFunc(v => typeof(v) == "string" &&
                                      /^[a-zA-Z0-9\-_]{12}$/.test(v)),
  index: simpleValidateFunc(v => Number.isInteger(v) &&
                                 v >= PlacesUtils.bookmarks.DEFAULT_INDEX),
  dateAdded: simpleValidateFunc(v => v.constructor.name == "Date"),
  lastModified: simpleValidateFunc(v => v.constructor.name == "Date"),
  type: simpleValidateFunc(v => Number.isInteger(v) &&
                                [ PlacesUtils.bookmarks.TYPE_BOOKMARK
                                , PlacesUtils.bookmarks.TYPE_FOLDER
                                , PlacesUtils.bookmarks.TYPE_SEPARATOR ].includes(v)),
  title: v => {
    simpleValidateFunc(val => val === null || typeof(val) == "string").call(this, v);
    if (!v)
      return null;
    return v.slice(0, DB_TITLE_LENGTH_MAX);
  },
  url: v => {
    simpleValidateFunc(val => (typeof(val) == "string" && val.length <= DB_URL_LENGTH_MAX) ||
                              (val instanceof Ci.nsIURI && val.spec.length <= DB_URL_LENGTH_MAX) ||
                              (val instanceof URL && val.href.length <= DB_URL_LENGTH_MAX)
                      ).call(this, v);
    if (typeof(v) === "string")
      return new URL(v);
    if (v instanceof Ci.nsIURI)
      return new URL(v.spec);
    return v;
  },
  source: simpleValidateFunc(v => Number.isInteger(v) &&
                                  Object.values(PlacesUtils.bookmarks.SOURCES).includes(v)),
});

// Sync bookmark records can contain additional properties.
const SYNC_BOOKMARK_VALIDATORS = Object.freeze({
  // Sync uses Places GUIDs for all records except roots.
  syncId: simpleValidateFunc(v => typeof v == "string" && (
                                  (PlacesSyncUtils.bookmarks.ROOTS.includes(v) ||
                                   PlacesUtils.isValidGuid(v)))),
  parentSyncId: v => SYNC_BOOKMARK_VALIDATORS.syncId(v),
  // Sync uses kinds instead of types, which distinguish between livemarks,
  // queries, and smart bookmarks.
  kind: simpleValidateFunc(v => typeof v == "string" &&
                                Object.values(PlacesSyncUtils.bookmarks.KINDS).includes(v)),
  query: simpleValidateFunc(v => v === null || (typeof v == "string" && v)),
  folder: simpleValidateFunc(v => typeof v == "string" && v &&
                                  v.length <= Ci.nsITaggingService.MAX_TAG_LENGTH),
  tags: v => {
    if (v === null) {
      return [];
    }
    if (!Array.isArray(v)) {
      throw new Error("Invalid tag array");
    }
    for (let tag of v) {
      if (typeof tag != "string" || !tag ||
          tag.length > Ci.nsITaggingService.MAX_TAG_LENGTH) {
        throw new Error(`Invalid tag: ${tag}`);
      }
    }
    return v;
  },
  keyword: simpleValidateFunc(v => v === null || typeof v == "string"),
  description: simpleValidateFunc(v => v === null || typeof v == "string"),
  loadInSidebar: simpleValidateFunc(v => v === true || v === false),
  dateAdded: simpleValidateFunc(v => typeof v === "number"
    && v > PlacesSyncUtils.bookmarks.EARLIEST_BOOKMARK_TIMESTAMP),
  feed: v => v === null ? v : BOOKMARK_VALIDATORS.url(v),
  site: v => v === null ? v : BOOKMARK_VALIDATORS.url(v),
  title: BOOKMARK_VALIDATORS.title,
  url: BOOKMARK_VALIDATORS.url,
});

// Sync change records are passed between `PlacesSyncUtils` and the Sync
// bookmarks engine, and are used to update an item's sync status and change
// counter at the end of a sync.
const SYNC_CHANGE_RECORD_VALIDATORS = Object.freeze({
  modified: simpleValidateFunc(v => typeof v == "number" && v >= 0),
  counter: simpleValidateFunc(v => typeof v == "number" && v >= 0),
  status: simpleValidateFunc(v => typeof v == "number" &&
                                  Object.values(PlacesUtils.bookmarks.SYNC_STATUS).includes(v)),
  tombstone: simpleValidateFunc(v => v === true || v === false),
  synced: simpleValidateFunc(v => v === true || v === false),
});

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
  MOBILE_ROOT_ANNO: "mobile/bookmarksRoot",

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

  asContainer: aNode => asContainer(aNode),
  asQuery: aNode => asQuery(aNode),

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
   * Is a string a valid GUID?
   *
   * @param guid: (String)
   * @return (Boolean)
   */
  isValidGuid(guid) {
    return typeof guid == "string" && guid &&
           (/^[a-zA-Z0-9\-_]{12}$/.test(guid));
  },

  /**
   * Converts a string or n URL object to an nsIURI.
   *
   * @param url (URL) or (String)
   *        the URL to convert.
   * @return nsIURI for the given URL.
   */
  toURI(url) {
    url = (url instanceof URL) ? url.href : url;

    return NetUtil.newURI(url);
  },

  /**
   * Convert a Date object to a PRTime (microseconds).
   *
   * @param date
   *        the Date object to convert.
   * @return microseconds from the epoch.
   */
  toPRTime(date) {
    return date * 1000;
  },

  /**
   * Convert a PRTime to a Date object.
   *
   * @param time
   *        microseconds from the epoch.
   * @return a Date object.
   */
  toDate(time) {
    return new Date(parseInt(time / 1000));
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
   * Makes a moz-action URI for the given action and set of parameters.
   *
   * @param   type
   *          The action type.
   * @param   params
   *          A JS object of action params.
   * @returns A moz-action URI as a string.
   */
  mozActionURI(type, params) {
    let encodedParams = {};
    for (let key in params) {
      // Strip null or undefined.
      // Regardless, don't encode them or they would be converted to a string.
      if (params[key] === null || params[key] === undefined) {
        continue;
      }
      encodedParams[key] = encodeURIComponent(params[key]);
    }
    return "moz-action:" + type + "," + JSON.stringify(encodedParams);
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
  nodeAncestors: function* PU_nodeAncestors(aNode) {
    let node = aNode.parent;
    while (node) {
      yield node;
      node = node.parent;
    }
  },

  /**
   * Checks validity of an object, filling up default values for optional
   * properties.
   *
   * @param validators (object)
   *        An object containing input validators. Keys should be field names;
   *        values should be validation functions.
   * @param props (object)
   *        The object to validate.
   * @param behavior (object) [optional]
   *        Object defining special behavior for some of the properties.
   *        The following behaviors may be optionally set:
   *         - required: this property is required.
   *         - replaceWith: this property will be overwritten with the value
   *                        provided
   *         - requiredIf: if the provided condition is satisfied, then this
   *                       property is required.
   *         - validIf: if the provided condition is not satisfied, then this
   *                    property is invalid.
   *         - defaultValue: an undefined property should default to this value.
   *
   * @return a validated and normalized item.
   * @throws if the object contains invalid data.
   * @note any unknown properties are pass-through.
   */
  validateItemProperties(validators, props, behavior = {}) {
    if (!props)
      throw new Error("Input should be a valid object");
    // Make a shallow copy of `props` to avoid mutating the original object
    // when filling in defaults.
    let input = Object.assign({}, props);
    let normalizedInput = {};
    let required = new Set();
    for (let prop in behavior) {
      if (behavior[prop].hasOwnProperty("required") && behavior[prop].required) {
        required.add(prop);
      }
      if (behavior[prop].hasOwnProperty("requiredIf") && behavior[prop].requiredIf(input)) {
        required.add(prop);
      }
      if (behavior[prop].hasOwnProperty("validIf") && input[prop] !== undefined &&
          !behavior[prop].validIf(input)) {
        throw new Error(`Invalid value for property '${prop}': ${JSON.stringify(input[prop])}`);
      }
      if (behavior[prop].hasOwnProperty("defaultValue") && input[prop] === undefined) {
        input[prop] = behavior[prop].defaultValue;
      }
      if (behavior[prop].hasOwnProperty("replaceWith")) {
        input[prop] = behavior[prop].replaceWith;
      }
    }

    for (let prop in input) {
      if (required.has(prop)) {
        required.delete(prop);
      } else if (input[prop] === undefined) {
        // Skip undefined properties that are not required.
        continue;
      }
      if (validators.hasOwnProperty(prop)) {
        try {
          normalizedInput[prop] = validators[prop](input[prop], input);
        } catch (ex) {
          throw new Error(`Invalid value for property '${prop}': ${input[prop]}`);
        }
      }
    }
    if (required.size > 0)
      throw new Error(`The following properties were expected: ${[...required].join(", ")}`);
    return normalizedInput;
  },

  BOOKMARK_VALIDATORS,
  SYNC_BOOKMARK_VALIDATORS,
  SYNC_CHANGE_RECORD_VALIDATORS,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver
  , Ci.nsITransactionListener
  ]),

  _shutdownFunctions: [],
  registerShutdownFunction: function PU_registerShutdownFunction(aFunc) {
    // If this is the first registered function, add the shutdown observer.
    if (this._shutdownFunctions.length == 0) {
      Services.obs.addObserver(this, this.TOPIC_SHUTDOWN);
    }
    this._shutdownFunctions.push(aFunc);
  },

  // nsIObserver
  observe: function PU_observe(aSubject, aTopic, aData) {
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

        // Initialize the keywords cache to start observing bookmarks
        // notifications.  This is needed as far as we support both the old and
        // the new bookmarking APIs at the same time.
        gKeywordsCachePromise.catch(Cu.reportError);
        break;
    }
  },

  onPageAnnotationSet() {},
  onPageAnnotationRemoved() {},


  // nsITransactionListener

  didDo: function PU_didDo(aManager, aTransaction, aDoResult) {
    updateCommandsOnActiveWindow();
  },

  didUndo: function PU_didUndo(aManager, aTransaction, aUndoResult) {
    updateCommandsOnActiveWindow();
  },

  didRedo: function PU_didRedo(aManager, aTransaction, aRedoResult) {
    updateCommandsOnActiveWindow();
  },

  didBeginBatch: function PU_didBeginBatch(aManager, aResult) {
    // A no-op transaction is pushed to the stack, in order to make safe and
    // easy to implement "Undo" an unknown number of transactions (including 0),
    // "above" beginBatch and endBatch. Otherwise,implementing Undo that way
    // head to dataloss: for example, if no changes were done in the
    // edit-item panel, the last transaction on the undo stack would be the
    // initial createItem transaction, or even worse, the batched editing of
    // some other item.
    // DO NOT MOVE this to the window scope, that would leak (bug 490068)!
    this.transactionManager.doTransaction({ doTransaction() {},
                                            undoTransaction() {},
                                            redoTransaction() {},
                                            isTransient: false,
                                            merge() { return false; }
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
    return this.containerTypes.includes(aNode.type);
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
   * Gets the concrete item-guid for the given node. For everything but folder
   * shortcuts, this is just node.bookmarkGuid.  For folder shortcuts, this is
   * node.targetFolderGuid (see nsINavHistoryService.idl for the semantics).
   *
   * @param aNode
   *        a result node.
   * @return the concrete item-guid for aNode.
   * @note unlike getConcreteItemId, this doesn't allow retrieving the guid of a
   *       ta container.
   */
  getConcreteItemGuid(aNode) {
    if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT)
      return asQuery(aNode).targetFolderGuid;
    return aNode.bookmarkGuid;
  },

  /**
   * Reverse a host based on the moz_places algorithm, that is reverse the host
   * string and add a trailing period.  For example "google.com" becomes
   * "moc.elgoog.".
   *
   * @param url
   *        the URL to generate a rev host for.
   * @return the reversed host string.
   */
  getReversedHost(url) {
    return url.host.split("").reverse().join("") + ".";
  },

  /**
   * String-wraps a result node according to the rules of the specified
   * content type for copy or move operations.
   *
   * @param   aNode
   *          The Result node to wrap (serialize)
   * @param   aType
   *          The content type to serialize as
   * @param   [optional] aFeedURI
   *          Used instead of the node's URI if provided.
   *          This is useful for wrapping a livemark as TYPE_X_MOZ_URL,
   *          TYPE_HTML or TYPE_UNICODE.
   * @return  A string serialization of the node
   */
  wrapNode(aNode, aType, aFeedURI) {
    // when wrapping a node, we want all the items, even if the original
    // query options are excluding them.
    // This can happen when copying from the left hand pane of the bookmarks
    // organizer.
    // @return [node, shouldClose]
    function gatherDataFromNode(node, gatherDataFunc) {
      if (PlacesUtils.nodeIsFolder(node) &&
          node.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT &&
          asQuery(node).queryOptions.excludeItems) {
        let folderRoot = PlacesUtils.getFolderContents(node.itemId, false, true).root;
        try {
          return gatherDataFunc(folderRoot);
        } finally {
          folderRoot.containerOpen = false;
        }
      }
      // If we didn't create our own query, do not alter the node's state.
      return gatherDataFunc(node);
    }

    function gatherDataHtml(node) {
      let htmlEscape = s => s.replace(/&/g, "&amp;")
                             .replace(/>/g, "&gt;")
                             .replace(/</g, "&lt;")
                             .replace(/"/g, "&quot;")
                             .replace(/'/g, "&apos;");

      // escape out potential HTML in the title
      let escapedTitle = node.title ? htmlEscape(node.title) : "";

      if (aFeedURI) {
        return `<A HREF="${aFeedURI}">${escapedTitle}</A>${NEWLINE}`;
      }

      if (PlacesUtils.nodeIsContainer(node)) {
        asContainer(node);
        let wasOpen = node.containerOpen;
        if (!wasOpen)
          node.containerOpen = true;

        let childString = "<DL><DT>" + escapedTitle + "</DT>" + NEWLINE;
        let cc = node.childCount;
        for (let i = 0; i < cc; ++i) {
          childString += "<DD>"
                         + NEWLINE
                         + gatherDataHtml(node.getChild(i))
                         + "</DD>"
                         + NEWLINE;
        }
        node.containerOpen = wasOpen;
        return childString + "</DL>" + NEWLINE;
      }
      if (PlacesUtils.nodeIsURI(node))
        return `<A HREF="${node.uri}">${escapedTitle}</A>${NEWLINE}`;
      if (PlacesUtils.nodeIsSeparator(node))
        return "<HR>" + NEWLINE;
      return "";
    }

    function gatherDataText(node) {
      if (aFeedURI) {
        return aFeedURI;
      }

      if (PlacesUtils.nodeIsContainer(node)) {
        asContainer(node);
        let wasOpen = node.containerOpen;
        if (!wasOpen)
          node.containerOpen = true;

        let childString = node.title + NEWLINE;
        let cc = node.childCount;
        for (let i = 0; i < cc; ++i) {
          let child = node.getChild(i);
          let suffix = i < (cc - 1) ? NEWLINE : "";
          childString += gatherDataText(child) + suffix;
        }
        node.containerOpen = wasOpen;
        return childString;
      }
      if (PlacesUtils.nodeIsURI(node))
        return node.uri;
      if (PlacesUtils.nodeIsSeparator(node))
        return "--------------------";
      return "";
    }

    switch (aType) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER: {
        // Serialize the node to JSON.
        return serializeNode(aNode, aFeedURI);
      }
      case this.TYPE_X_MOZ_URL: {
        if (aFeedURI || PlacesUtils.nodeIsURI(aNode))
          return (aFeedURI || aNode.uri) + NEWLINE + aNode.title;
        return "";
      }
      case this.TYPE_HTML: {
        return gatherDataFromNode(aNode, gatherDataHtml);
      }
    }

    // Otherwise, we wrap as TYPE_UNICODE.
    return gatherDataFromNode(aNode, gatherDataText);
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
    switch (type) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        nodes = JSON.parse("[" + blob + "]");
        break;
      case this.TYPE_X_MOZ_URL: {
        let parts = blob.split("\n");
        // data in this type has 2 parts per entry, so if there are fewer
        // than 2 parts left, the blob is malformed and we should stop
        // but drag and drop of files from the shell has parts.length = 1
        if (parts.length != 1 && parts.length % 2)
          break;
        for (let i = 0; i < parts.length; i = i + 2) {
          let uriString = parts[i];
          let titleString = "";
          if (parts.length > i + 1)
            titleString = parts[i + 1];
          else {
            // for drag and drop of files, try to use the leafName as title
            try {
              titleString = this._uri(uriString).QueryInterface(Ci.nsIURL)
                                .fileName;
            } catch (e) {}
          }
          // note:  this._uri() will throw if uriString is not a valid URI
          if (this._uri(uriString)) {
            nodes.push({ uri: uriString,
                         title: titleString ? titleString : uriString,
                         type: this.TYPE_X_MOZ_URL });
          }
        }
        break;
      }
      case this.TYPE_UNICODE: {
        let parts = blob.split("\n");
        for (let i = 0; i < parts.length; i++) {
          let uriString = parts[i];
          // text/uri-list is converted to TYPE_UNICODE but it could contain
          // comments line prepended by #, we should skip them
          if (uriString.substr(0, 1) == "\x23")
            continue;
          // note: this._uri() will throw if uriString is not a valid URI
          if (uriString != "" && this._uri(uriString))
            nodes.push({ uri: uriString,
                         title: uriString,
                         type: this.TYPE_X_MOZ_URL });
        }
        break;
      }
      default:
        throw Cr.NS_ERROR_INVALID_ARG;
    }
    return nodes;
  },

  /**
   * Validate an input PageInfo object, returning a valid PageInfo object.
   *
   * @param pageInfo: (PageInfo)
   * @return (PageInfo)
   */
  validatePageInfo(pageInfo, validateVisits = true) {
    let info = {
      visits: [],
    };

    if (!pageInfo.url) {
      throw new TypeError("PageInfo object must have a url property");
    }

    info.url = this.normalizeToURLOrGUID(pageInfo.url);

    if (!validateVisits) {
      return info;
    }

    if (typeof pageInfo.title === "string") {
      info.title = pageInfo.title;
    } else if (pageInfo.title != null && pageInfo.title != undefined) {
      throw new TypeError(`title property of PageInfo object: ${pageInfo.title} must be a string if provided`);
    }

    if (!pageInfo.visits || !Array.isArray(pageInfo.visits) || !pageInfo.visits.length) {
      throw new TypeError("PageInfo object must have an array of visits");
    }

    for (let inVisit of pageInfo.visits) {
      let visit = {
        date: new Date(),
        transition: inVisit.transition || History.TRANSITIONS.LINK,
      };

      if (!PlacesUtils.history.isValidTransition(visit.transition)) {
        throw new TypeError(`transition: ${visit.transition} is not a valid transition type`);
      }

      if (inVisit.date) {
        PlacesUtils.history.ensureDate(inVisit.date);
        if (inVisit.date > (Date.now() + TIMERS_RESOLUTION_SKEW_MS)) {
          throw new TypeError(`date: ${inVisit.date} cannot be a future date`);
        }
        visit.date = inVisit.date;
      }

      if (inVisit.referrer) {
        visit.referrer = this.normalizeToURLOrGUID(inVisit.referrer);
      }
      info.visits.push(visit);
    }
    return info;
  },

  /**
   * Normalize a key to either a string (if it is a valid GUID) or an
   * instance of `URL` (if it is a `URL`, `nsIURI`, or a string
   * representing a valid url).
   *
   * @throws (TypeError)
   *         If the key is neither a valid guid nor a valid url.
   */
  normalizeToURLOrGUID(key) {
    if (typeof key === "string") {
      // A string may be a URL or a guid
      if (this.isValidGuid(key)) {
        return key;
      }
      return new URL(key);
    }
    if (key instanceof URL) {
      return key;
    }
    if (key instanceof Ci.nsIURI) {
      return new URL(key.spec);
    }
    throw new TypeError("Invalid url or guid: " + key);
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
      } else {
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
  setAnnotationsForItem: function PU_setAnnotationsForItem(aItemId, aAnnos, aSource) {
    var annosvc = this.annotations;

    aAnnos.forEach(function(anno) {
      if (anno.value === undefined || anno.value === null) {
        annosvc.removeItemAnnotation(aItemId, anno.name, aSource);
      } else {
        let flags = ("flags" in anno) ? anno.flags : 0;
        let expires = ("expires" in anno) ?
          anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
        annosvc.setItemAnnotation(aItemId, anno.name, anno.value, flags,
                                  expires, aSource);
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

  get mobileFolderId() {
    delete this.mobileFolderId;
    return this.mobileFolderId = this.bookmarks.mobileFolder;
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
           aItemId == PlacesUtils.placesRootId ||
           aItemId == PlacesUtils.mobileFolderId;
  },

  /**
   * Set the POST data associated with a bookmark, if any.
   * Used by POST keywords.
   *   @param aBookmarkId
   *
   * @deprecated Use PlacesUtils.keywords.insert() API instead.
   */
  setPostDataForBookmark(aBookmarkId, aPostData) {
    if (!aPostData)
      throw new Error("Must provide valid POST data");
    // For now we don't have a unified API to create a keyword with postData,
    // thus here we can just try to complete a keyword that should already exist
    // without any post data.
    let stmt = PlacesUtils.history.DBConnection.createStatement(
      `UPDATE moz_keywords SET post_data = :post_data
       WHERE id = (SELECT k.id FROM moz_keywords k
                   JOIN moz_bookmarks b ON b.fk = k.place_id
                   WHERE b.id = :item_id
                   AND post_data ISNULL
                   LIMIT 1)`);
    stmt.params.item_id = aBookmarkId;
    stmt.params.post_data = aPostData;
    try {
      stmt.execute();
    } finally {
      stmt.finalize();
    }

    // Update the cache.
    return (async function() {
      let guid = await PlacesUtils.promiseItemGuid(aBookmarkId);
      let bm = await PlacesUtils.bookmarks.fetch(guid);

      // Fetch keywords for this href.
      let cache = await gKeywordsCachePromise;
      for (let [ , entry ] of cache) {
        // Set the POST data on keywords not having it.
        if (entry.url.href == bm.url.href && !entry.postData) {
          entry.postData = aPostData;
        }
      }
    })().catch(Cu.reportError);
  },

  /**
   * Get the POST data associated with a bookmark, if any.
   * @param aBookmarkId
   * @returns string of POST data if set for aBookmarkId. null otherwise.
   *
   * @deprecated Use PlacesUtils.keywords.fetch() API instead.
   */
  getPostDataForBookmark(aBookmarkId) {
    let stmt = PlacesUtils.history.DBConnection.createStatement(
      `SELECT k.post_data
       FROM moz_keywords k
       JOIN moz_places h ON h.id = k.place_id
       JOIN moz_bookmarks b ON b.fk = h.id
       WHERE b.id = :item_id`);
    stmt.params.item_id = aBookmarkId;
    try {
      if (!stmt.executeStep())
        return null;
      return stmt.row.post_data;
    } finally {
      stmt.finalize();
    }
  },

  /**
   * Get the URI (and any associated POST data) for a given keyword.
   * @param aKeyword string keyword
   * @returns an array containing a string URL and a string of POST data
   *
   * @deprecated
   */
  getURLAndPostDataForKeyword(aKeyword) {
    Deprecated.warning("getURLAndPostDataForKeyword() is deprecated, please " +
                       "use PlacesUtils.keywords.fetch() instead",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1100294");

    let stmt = PlacesUtils.history.DBConnection.createStatement(
      `SELECT h.url, k.post_data
       FROM moz_keywords k
       JOIN moz_places h ON h.id = k.place_id
       WHERE k.keyword = :keyword`);
    stmt.params.keyword = aKeyword.toLowerCase();
    try {
      if (!stmt.executeStep())
        return [ null, null ];
      return [ stmt.row.url, stmt.row.post_data ];
    } finally {
      stmt.finalize();
    }
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
  hasChildURIs: function PU_hasChildURIs(aNode) {
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

    let found = false;
    for (let i = 0; i < root.childCount && !found; i++) {
      let child = root.getChild(i);
      if (this.nodeIsURI(child))
        found = true;
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
   * Gets a shared Sqlite.jsm readonly connection to the Places database,
   * usable only for SELECT queries.
   *
   * This is intended to be used mostly internally, components outside of
   * Places should, when possible, use API calls and file bugs to get proper
   * APIs, where they are missing.
   * Keep in mind the Places DB schema is by no means frozen or even stable.
   * Your custom queries can - and will - break overtime.
   *
   * Example:
   * let db = await PlacesUtils.promiseDBConnection();
   * let rows = await db.executeCached(sql, params);
   */
  promiseDBConnection: () => gAsyncDBConnPromised,

  /**
   * Performs a read/write operation on the Places database through a Sqlite.jsm
   * wrapped connection to the Places database.
   *
   * This is intended to be used only by Places itself, always use APIs if you
   * need to modify the Places database. Use promiseDBConnection if you need to
   * SELECT from the database and there's no covering API.
   * Keep in mind the Places DB schema is by no means frozen or even stable.
   * Your custom queries can - and will - break overtime.
   *
   * As all operations on the Places database are asynchronous, if shutdown
   * is initiated while an operation is pending, this could cause dataloss.
   * Using `withConnectionWrapper` ensures that shutdown waits until all
   * operations are complete before proceeding.
   *
   * Example:
   * await withConnectionWrapper("Bookmarks: Remove a bookmark", Task.async(function*(db) {
   *    // Proceed with the db, asynchronously.
   *    // Shutdown will not interrupt operations that take place here.
   * }));
   *
   * @param {string} name The name of the operation. Used for debugging, logging
   *   and crash reporting.
   * @param {function(db)} task A function that takes as argument a Sqlite.jsm
   *   connection and returns a Promise. Shutdown is guaranteed to not interrupt
   *   execution of `task`.
   */
  withConnectionWrapper: (name, task) => {
    if (!name) {
      throw new TypeError("Expecting a user-readable name");
    }
    return (async function() {
      let db = await gAsyncDBWrapperPromised;
      return db.executeBeforeShutdown(name, task);
    })();
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
    return new Promise(resolve => {

      // Delaying to catch issues with asynchronous behavior while waiting
      // to implement asynchronous annotations in bug 699844.
      Services.tm.dispatchToMainThread(function() {
        if (aCharset && aCharset.length > 0) {
          PlacesUtils.annotations.setPageAnnotation(
            aURI, PlacesUtils.CHARSET_ANNO, aCharset, 0,
            Ci.nsIAnnotationService.EXPIRE_NEVER);
        } else {
          PlacesUtils.annotations.removePageAnnotation(
            aURI, PlacesUtils.CHARSET_ANNO);
        }
        resolve();
      });

    });
  },

  /**
   * Gets the last saved character-set for a URI.
   *
   * @param aURI nsIURI
   * @return {Promise}
   * @resolve a character-set or null.
   */
  getCharsetForURI: function PU_getCharsetForURI(aURI) {
    return new Promise(resolve => {

      Services.tm.dispatchToMainThread(function() {
        let charset = null;

        try {
          charset = PlacesUtils.annotations.getPageAnnotation(aURI,
                                                              PlacesUtils.CHARSET_ANNO);
        } catch (ex) { }

        resolve(charset);
      });

    });
  },

  /**
   * Promised wrapper for mozIAsyncHistory::getPlacesInfo for a single place.
   *
   * @param aPlaceIdentifier
   *        either an nsIURI or a GUID (@see getPlacesInfo)
   * @resolves to the place info object handed to handleResult.
   */
  promisePlaceInfo: function PU_promisePlaceInfo(aPlaceIdentifier) {
    return new Promise((resolve, reject) => {
      PlacesUtils.asyncHistory.getPlacesInfo(aPlaceIdentifier, {
        _placeInfo: null,
        handleResult: function handleResult(aPlaceInfo) {
          this._placeInfo = aPlaceInfo;
        },
        handleError: function handleError(aResultCode, aPlaceInfo) {
          reject(new Components.Exception("Error", aResultCode));
        },
        handleCompletion() {
          resolve(this._placeInfo);
        }
      });

    });
  },

  /**
   * Gets favicon data for a given page url.
   *
   * @param aPageUrl url of the page to look favicon for.
   * @resolves to an object representing a favicon entry, having the following
   *           properties: { uri, dataLen, data, mimeType }
   * @rejects JavaScript exception if the given url has no associated favicon.
   */
  promiseFaviconData(aPageUrl) {
    return new Promise((resolve, reject) => {
      PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(aPageUrl),
        function(aURI, aDataLen, aData, aMimeType) {
          if (aURI) {
            resolve({ uri: aURI,
                               dataLen: aDataLen,
                               data: aData,
                               mimeType: aMimeType });
          } else {
            reject();
          }
        });
    });
  },

  /**
   * Gets the favicon link url (moz-anno:) for a given page url.
   *
   * @param aPageURL url of the page to lookup the favicon for.
   * @resolves to the nsIURL of the favicon link
   * @rejects if the given url has no associated favicon.
   */
  promiseFaviconLinkUrl(aPageUrl) {
    return new Promise((resolve, reject) => {
      if (!(aPageUrl instanceof Ci.nsIURI))
        aPageUrl = NetUtil.newURI(aPageUrl);

      PlacesUtils.favicons.getFaviconURLForPage(aPageUrl, uri => {
        if (uri) {
          uri = PlacesUtils.favicons.getFaviconLinkForIcon(uri);
          resolve(uri);
        } else {
          reject("favicon not found for uri");
        }
      });
    });
  },

   /**
   * Returns the passed URL with a #size ref for the specified size and
   * devicePixelRatio.
   *
   * @param window
   *        The window where the icon will appear.
   * @param href
   *        The string href we should add the ref to.
   * @param size
   *        The target image size
   * @return The URL with the fragment at the end, in the same formar as input.
   */
  urlWithSizeRef(window, href, size) {
    return href + (href.includes("#") ? "&" : "#") +
           "size=" + (Math.round(size) * window.devicePixelRatio);
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
  promiseItemGuid(aItemId) {
    return GuidHelper.getItemGuid(aItemId)
  },

  /**
   * Get the item id for an item (a bookmark, a folder or a separator) given
   * its unique id.
   *
   * @param aGuid
   *        an item GUID
   * @return {Promise}
   * @resolves to the GUID.
   * @rejects if there's no item for the given GUID.
   */
  promiseItemId(aGuid) {
    return GuidHelper.getItemId(aGuid)
  },

  promiseManyItemIds(aGuids) {
    return GuidHelper.getManyItemIds(aGuids);
  },

  /**
   * Invalidate the GUID cache for the given itemId.
   *
   * @param aItemId
   *        an item id
   */
  invalidateCachedGuidFor(aItemId) {
    GuidHelper.invalidateCacheForItemId(aItemId)
  },

  /**
   * Asynchronously retrieve a JS-object representation of a places bookmarks
   * item (a bookmark, a folder, or a separator) along with all of its
   * descendants.
   *
   * @param [optional] aItemGuid
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
   *        - includeItemIds: opt-in to include the deprecated id property.
   *          Use it if you must. It'll be removed once the switch to GUIDs is
   *          complete.
   *
   * @return {Promise}
   * @resolves to a JS object that represents either a single item or a
   * bookmarks tree.  Each node in the tree has the following properties set:
   *  - guid (string): the item's GUID (same as aItemGuid for the top item).
   *  - [deprecated] id (number): the item's id. This is only if
   *    aOptions.includeItemIds is set.
   *  - type (string):  the item's type.  @see PlacesUtils.TYPE_X_*
   *  - title (string): the item's title. If it has no title, this property
   *    isn't set.
   *  - dateAdded (number, microseconds from the epoch): the date-added value of
   *    the item.
   *  - lastModified (number, microseconds from the epoch): the last-modified
   *    value of the item.
   *  - annos (see getAnnotationsForItem): the item's annotations.  This is not
   *    set if there are no annotations set for the item).
   *  - index: the item's index under it's parent.
   *
   * The root object (i.e. the one for aItemGuid) also has the following
   * properties set:
   *  - parentGuid (string): the GUID of the root's parent.  This isn't set if
   *    the root item is the places root.
   *  - itemsCount (number, not enumerable): the number of items, including the
   *    root item itself, which are represented in the resolved object.
   *
   * Bookmark items also have the following properties:
   *  - uri (string): the item's url.
   *  - tags (string): csv string of the bookmark's tags.
   *  - charset (string): the last known charset of the bookmark.
   *  - keyword (string): the bookmark's keyword (unset if none).
   *  - postData (string): the bookmark's keyword postData (unset if none).
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
   * @note if aItemGuid points to a non-existent item, the returned promise is
   * resolved to null.
   */
  async promiseBookmarksTree(aItemGuid = "", aOptions = {}) {
    let createItemInfoObject = async function(aRow, aIncludeParentGuid) {
      let item = {};
      let copyProps = (...props) => {
        for (let prop of props) {
          let val = aRow.getResultByName(prop);
          if (val !== null)
            item[prop] = val;
        }
      };
      copyProps("guid", "title", "index", "dateAdded", "lastModified");
      if (aIncludeParentGuid)
        copyProps("parentGuid");

      let itemId = aRow.getResultByName("id");
      if (aOptions.includeItemIds)
        item.id = itemId;

      // Cache it for promiseItemId consumers regardless.
      GuidHelper.updateCache(itemId, item.guid);

      let type = aRow.getResultByName("type");
      if (type == Ci.nsINavBookmarksService.TYPE_BOOKMARK)
        copyProps("charset", "tags", "iconuri");

      // Add annotations.
      if (aRow.getResultByName("has_annos")) {
        try {
          item.annos = PlacesUtils.getAnnotationsForItem(itemId);
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
          let entry = await PlacesUtils.keywords.fetch({ url: item.uri });
          if (entry) {
            item.keyword = entry.keyword;
            item.postData = entry.postData;
          }
          break;
        case Ci.nsINavBookmarksService.TYPE_FOLDER:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
          // Mark root folders.
          if (itemId == PlacesUtils.placesRootId)
            item.root = "placesRoot";
          else if (itemId == PlacesUtils.bookmarksMenuFolderId)
            item.root = "bookmarksMenuFolder";
          else if (itemId == PlacesUtils.unfiledBookmarksFolderId)
            item.root = "unfiledBookmarksFolder";
          else if (itemId == PlacesUtils.toolbarFolderId)
            item.root = "toolbarFolder";
          else if (itemId == PlacesUtils.mobileFolderId)
            item.root = "mobileFolder";
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
      `/* do not warn (bug no): cannot use an index */
       WITH RECURSIVE
       descendants(fk, level, type, id, guid, parent, parentGuid, position,
                   title, dateAdded, lastModified) AS (
         SELECT b1.fk, 0, b1.type, b1.id, b1.guid, b1.parent,
                (SELECT guid FROM moz_bookmarks WHERE id = b1.parent),
                b1.position, b1.title, b1.dateAdded, b1.lastModified
         FROM moz_bookmarks b1 WHERE b1.guid=:item_guid
         UNION ALL
         SELECT b2.fk, level + 1, b2.type, b2.id, b2.guid, b2.parent,
                descendants.guid, b2.position, b2.title, b2.dateAdded,
                b2.lastModified
         FROM moz_bookmarks b2
         JOIN descendants ON b2.parent = descendants.id AND b2.id <> :tags_folder)
       SELECT d.level, d.id, d.guid, d.parent, d.parentGuid, d.type,
              d.position AS [index], d.title, d.dateAdded, d.lastModified,
              h.url, (SELECT icon_url FROM moz_icons i
                      JOIN moz_icons_to_pages ON icon_id = i.id
                      JOIN moz_pages_w_icons pi ON page_id = pi.id
                      WHERE pi.page_url_hash = hash(h.url) AND pi.page_url = h.url
                      ORDER BY width DESC LIMIT 1) AS iconuri,
              (SELECT GROUP_CONCAT(t.title, ',')
               FROM moz_bookmarks b2
               JOIN moz_bookmarks t ON t.id = +b2.parent AND t.parent = :tags_folder
               WHERE b2.fk = h.id
              ) AS tags,
              EXISTS (SELECT 1 FROM moz_items_annos
                      WHERE item_id = d.id LIMIT 1) AS has_annos,
              (SELECT a.content FROM moz_annos a
               JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id
               WHERE place_id = h.id AND n.name = :charset_anno
              ) AS charset
       FROM descendants d
       LEFT JOIN moz_bookmarks b3 ON b3.id = d.parent
       LEFT JOIN moz_places h ON h.id = d.fk
       ORDER BY d.level, d.parent, d.position`;


    if (!aItemGuid)
      aItemGuid = this.bookmarks.rootGuid;

    let hasExcludeItemsCallback =
      aOptions.hasOwnProperty("excludeItemsCallback");
    let excludedParents = new Set();
    let shouldExcludeItem = (aItem, aParentGuid) => {
      let exclude = excludedParents.has(aParentGuid) ||
                    aOptions.excludeItemsCallback(aItem);
      if (exclude) {
        if (aItem.type == this.TYPE_X_MOZ_PLACE_CONTAINER)
          excludedParents.add(aItem.guid);
      }
      return exclude;
    };

    let rootItem = null;
    let parentsMap = new Map();
    let conn = await this.promiseDBConnection();
    let rows = await conn.executeCached(QUERY_STR,
        { tags_folder: PlacesUtils.tagsFolderId,
          charset_anno: PlacesUtils.CHARSET_ANNO,
          item_guid: aItemGuid });
    let yieldCounter = 0;
    for (let row of rows) {
      let item;
      if (!rootItem) {
        try {
          // This is the first row.
          rootItem = item = await createItemInfoObject(row, true);
          Object.defineProperty(rootItem, "itemsCount", { value: 1
                                                        , writable: true
                                                        , enumerable: false
                                                        , configurable: false });
        } catch (ex) {
          throw new Error("Failed to fetch the data for the root item " + ex);
        }
      } else {
        try {
          // Our query guarantees that we always visit parents ahead of their
          // children.
          item = await createItemInfoObject(row, false);
          let parentGuid = row.getResultByName("parentGuid");
          if (hasExcludeItemsCallback && shouldExcludeItem(item, parentGuid))
            continue;

          let parentItem = parentsMap.get(parentGuid);
          if ("children" in parentItem)
            parentItem.children.push(item);
          else
            parentItem.children = [item];

          rootItem.itemsCount++;
        } catch (ex) {
          // This is a bogus child, report and skip it.
          Cu.reportError("Failed to fetch the data for an item " + ex);
          continue;
        }
      }

      if (item.type == this.TYPE_X_MOZ_PLACE_CONTAINER)
        parentsMap.set(item.guid, item);

      // With many bookmarks we end up stealing the CPU - even with yielding!
      // So we let everyone else have a go every few items (bug 1186714).
      if (++yieldCounter % 50 == 0) {
        await new Promise(resolve => {
          Services.tm.dispatchToMainThread(resolve);
        });
      }
    }

    return rootItem;
  }
};

XPCOMUtils.defineLazyGetter(PlacesUtils, "history", function() {
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"]
             .getService(Ci.nsINavHistoryService)
             .QueryInterface(Ci.nsIBrowserHistory)
             .QueryInterface(Ci.nsPIPlacesDatabase);
  return Object.freeze(new Proxy(hs, {
    get(target, name) {
      let property, object;
      if (name in target) {
        property = target[name];
        object = target;
      } else {
        property = History[name];
        object = History;
      }
      if (typeof property == "function") {
        return property.bind(object);
      }
      return property;
    }
  }));
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

XPCOMUtils.defineLazyGetter(PlacesUtils, "bookmarks", () => {
  let bm = Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
             .getService(Ci.nsINavBookmarksService);
  return Object.freeze(new Proxy(bm, {
    get: (target, name) => target.hasOwnProperty(name) ? target[name]
                                                       : Bookmarks[name]
  }));
});

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "annotations",
                                   "@mozilla.org/browser/annotation-service;1",
                                   "nsIAnnotationService");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "tagging",
                                   "@mozilla.org/browser/tagging-service;1",
                                   "nsITaggingService");

XPCOMUtils.defineLazyServiceGetter(PlacesUtils, "livemarks",
                                   "@mozilla.org/browser/livemark-service;2",
                                   "mozIAsyncLivemarks");

XPCOMUtils.defineLazyGetter(PlacesUtils, "keywords", () => Keywords);

XPCOMUtils.defineLazyGetter(PlacesUtils, "transactionManager", function() {
  let tm = Cc["@mozilla.org/transactionmanager;1"].
           createInstance(Ci.nsITransactionManager);
  tm.AddListener(PlacesUtils);
  this.registerShutdownFunction(function() {
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
      value(aTransaction) {
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

/**
 * Setup internal databases for closing properly during shutdown.
 *
 * 1. Places initiates shutdown.
 * 2. Before places can move to the step where it closes the low-level connection,
 *   we need to make sure that we have closed `conn`.
 * 3. Before we can close `conn`, we need to make sure that all external clients
 *   have stopped using `conn`.
 * 4. Before we can close Sqlite, we need to close `conn`.
 */
function setupDbForShutdown(conn, name) {
  try {
    let state = "0. Not started.";
    let promiseClosed = new Promise((resolve, reject) => {
      // The service initiates shutdown.
      // Before it can safely close its connection, we need to make sure
      // that we have closed the high-level connection.
      try {
        AsyncShutdown.placesClosingInternalConnection.addBlocker(`${name} closing as part of Places shutdown`,
          async function() {
            state = "1. Service has initiated shutdown";

            // At this stage, all external clients have finished using the
            // database. We just need to close the high-level connection.
            await conn.close();
            state = "2. Closed Sqlite.jsm connection.";

            resolve();
          },
          () => state
        );
      } catch (ex) {
        // It's too late to block shutdown, just close the connection.
        conn.close();
        reject(ex);
      }
    });

    // Make sure that Sqlite.jsm doesn't close until we are done
    // with the high-level connection.
    Sqlite.shutdown.addBlocker(`${name} must be closed before Sqlite.jsm`,
      () => promiseClosed.catch(Cu.reportError),
      () => state
    );
  } catch (ex) {
    // It's too late to block shutdown, just close the connection.
    conn.close();
    throw ex;
  }
}

XPCOMUtils.defineLazyGetter(this, "gAsyncDBConnPromised",
  () => Sqlite.cloneStorageConnection({
    connection: PlacesUtils.history.DBConnection,
    readOnly:   true
  }).then(conn => {
      setupDbForShutdown(conn, "PlacesUtils read-only connection");
      return conn;
  }).catch(Cu.reportError)
);

XPCOMUtils.defineLazyGetter(this, "gAsyncDBWrapperPromised",
  () => Sqlite.wrapStorageConnection({
      connection: PlacesUtils.history.DBConnection,
  }).then(conn => {
    setupDbForShutdown(conn, "PlacesUtils wrapped connection");
    return conn;
  }).catch(Cu.reportError)
);

/**
 * Keywords management API.
 * Sooner or later these keywords will merge with search keywords, this is an
 * interim API that should then be replaced by a unified one.
 * Keywords are associated with URLs and can have POST data.
 * A single URL can have multiple keywords, provided they differ by POST data.
 */
var Keywords = {
  /**
   * Fetches a keyword entry based on keyword or URL.
   *
   * @param keywordOrEntry
   *        Either the keyword to fetch or an entry providing keyword
   *        or url property to find keywords for.  If both properties are set,
   *        this returns their intersection.
   * @param onResult [optional]
   *        Callback invoked for each found entry.
   * @return {Promise}
   * @resolves to an object in the form: { keyword, url, postData },
   *           or null if a keyword entry was not found.
   */
  fetch(keywordOrEntry, onResult = null) {
    if (typeof(keywordOrEntry) == "string")
      keywordOrEntry = { keyword: keywordOrEntry };

    if (keywordOrEntry === null || typeof(keywordOrEntry) != "object" ||
        (("keyword" in keywordOrEntry) && typeof(keywordOrEntry.keyword) != "string"))
      throw new Error("Invalid keyword");

    let hasKeyword = "keyword" in keywordOrEntry;
    let hasUrl = "url" in keywordOrEntry;

    if (!hasKeyword && !hasUrl)
      throw new Error("At least keyword or url must be provided");
    if (onResult && typeof onResult != "function")
      throw new Error("onResult callback must be a valid function");

    if (hasUrl)
      keywordOrEntry.url = new URL(keywordOrEntry.url);
    if (hasKeyword)
      keywordOrEntry.keyword = keywordOrEntry.keyword.trim().toLowerCase();

    let safeOnResult = entry => {
      if (onResult) {
        try {
          onResult(entry);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    };

    return gKeywordsCachePromise.then(cache => {
      let entries = [];
      if (hasKeyword) {
        let entry = cache.get(keywordOrEntry.keyword);
        if (entry)
          entries.push(entry);
      }
      if (hasUrl) {
        for (let entry of cache.values()) {
          if (entry.url.href == keywordOrEntry.url.href)
            entries.push(entry);
        }
      }

      entries = entries.filter(e => {
        return (!hasUrl || e.url.href == keywordOrEntry.url.href) &&
               (!hasKeyword || e.keyword == keywordOrEntry.keyword);
      });

      entries.forEach(safeOnResult);
      return entries.length ? entries[0] : null;
    });
  },

  /**
   * Adds a new keyword and postData for the given URL.
   *
   * @param keywordEntry
   *        An object describing the keyword to insert, in the form:
   *        {
   *          keyword: non-empty string,
   *          URL: URL or href to associate to the keyword,
   *          postData: optional POST data to associate to the keyword
   *        }
   * @note Do not define a postData property if there isn't any POST data.
   * @resolves when the addition is complete.
   */
  insert(keywordEntry) {
    if (!keywordEntry || typeof keywordEntry != "object")
      throw new Error("Input should be a valid object");

    if (!("keyword" in keywordEntry) || !keywordEntry.keyword ||
        typeof(keywordEntry.keyword) != "string")
      throw new Error("Invalid keyword");
    if (("postData" in keywordEntry) && keywordEntry.postData &&
                                        typeof(keywordEntry.postData) != "string")
      throw new Error("Invalid POST data");
    if (!("url" in keywordEntry))
      throw new Error("undefined is not a valid URL");
    let { keyword, url,
          source = Ci.nsINavBookmarksService.SOURCE_DEFAULT } = keywordEntry;
    keyword = keyword.trim().toLowerCase();
    let postData = keywordEntry.postData || null;
    // This also checks href for validity
    url = new URL(url);

    return PlacesUtils.withConnectionWrapper("Keywords.insert", async function(db) {
        let cache = await gKeywordsCachePromise;

        // Trying to set the same keyword is a no-op.
        let oldEntry = cache.get(keyword);
        if (oldEntry && oldEntry.url.href == url.href &&
                        oldEntry.postData == keywordEntry.postData) {
          return;
        }

        // A keyword can only be associated to a single page.
        // If another page is using the new keyword, we must update the keyword
        // entry.
        // Note we cannot use INSERT OR REPLACE cause it wouldn't invoke the delete
        // trigger.
        if (oldEntry) {
          await db.executeCached(
            `UPDATE moz_keywords
             SET place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
                 post_data = :post_data
             WHERE keyword = :keyword
            `, { url: url.href, keyword, post_data: postData });
          await notifyKeywordChange(oldEntry.url.href, "", source);
        } else {
          // An entry for the given page could be missing, in such a case we need to
          // create it.  The IGNORE conflict can trigger on `guid`.
          await db.executeCached(
            `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid)
             VALUES (:url, hash(:url), :rev_host, 0, :frecency,
                     IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
                            GENERATE_GUID()))
            `, { url: url.href, rev_host: PlacesUtils.getReversedHost(url),
                 frecency: url.protocol == "place:" ? 0 : -1 });
          await db.executeCached(
            `INSERT INTO moz_keywords (keyword, place_id, post_data)
             VALUES (:keyword, (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url), :post_data)
            `, { url: url.href, keyword, post_data: postData });
        }

        await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
          db, url, PlacesSyncUtils.bookmarks.determineSyncChangeDelta(source));

        cache.set(keyword, { keyword, url, postData });

        // In any case, notify about the new keyword.
        await notifyKeywordChange(url.href, keyword, source);
      }
    );
  },

  /**
   * Removes a keyword.
   *
   * @param keyword
   *        The keyword to remove.
   * @return {Promise}
   * @resolves when the removal is complete.
   */
  remove(keywordOrEntry) {
    if (typeof(keywordOrEntry) == "string")
      keywordOrEntry = { keyword: keywordOrEntry };

    if (keywordOrEntry === null || typeof(keywordOrEntry) != "object" ||
        !keywordOrEntry.keyword || typeof keywordOrEntry.keyword != "string")
      throw new Error("Invalid keyword");

    let { keyword,
          source = Ci.nsINavBookmarksService.SOURCE_DEFAULT } = keywordOrEntry;
    keyword = keywordOrEntry.keyword.trim().toLowerCase();
    return PlacesUtils.withConnectionWrapper("Keywords.remove", async function(db) {
      let cache = await gKeywordsCachePromise;
      if (!cache.has(keyword))
        return;
      let { url } = cache.get(keyword);
      cache.delete(keyword);

      await db.execute(`DELETE FROM moz_keywords WHERE keyword = :keyword`,
                       { keyword });

      await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
        db, url, PlacesSyncUtils.bookmarks.determineSyncChangeDelta(source));

      // Notify bookmarks about the removal.
      await notifyKeywordChange(url.href, "", source);
    });
  }
};

// Set by the keywords API to distinguish notifications fired by the old API.
// Once the old API will be gone, we can remove this and stop observing.
var gIgnoreKeywordNotifications = false;

XPCOMUtils.defineLazyGetter(this, "gKeywordsCachePromise", () =>
  PlacesUtils.withConnectionWrapper("PlacesUtils: gKeywordsCachePromise",
    async function(db) {
      let cache = new Map();
      let rows = await db.execute(
        `SELECT keyword, url, post_data
         FROM moz_keywords k
         JOIN moz_places h ON h.id = k.place_id
        `);
      let brokenKeywords = [];
      for (let row of rows) {
        let keyword = row.getResultByName("keyword");
        try {
          let entry = { keyword,
                        url: new URL(row.getResultByName("url")),
                        postData: row.getResultByName("post_data") };
          cache.set(keyword, entry);
        } catch (ex) {
          // The url is invalid, don't load the keyword and remove it, or it
          // would break the whole keywords API.
          brokenKeywords.push(keyword);
        }
      }
      if (brokenKeywords.length) {
        await db.execute(
          `DELETE FROM moz_keywords
           WHERE keyword IN (${brokenKeywords.map(JSON.stringify).join(",")})
          `);
      }

      // Helper to get a keyword from an href.
      function keywordsForHref(href) {
        let keywords = [];
        for (let [ key, val ] of cache) {
          if (val.url.href == href)
            keywords.push(key);
        }
        return keywords;
      }

      // Start observing changes to bookmarks. For now we are going to keep that
      // relation for backwards compatibility reasons, but mostly because we are
      // lacking a UI to manage keywords directly.
      let observer = {
        QueryInterface: XPCOMUtils.generateQI(Ci.nsINavBookmarkObserver),
        onBeginUpdateBatch() {},
        onEndUpdateBatch() {},
        onItemAdded() {},
        onItemVisited() {},
        onItemMoved() {},

        onItemRemoved(id, parentId, index, itemType, uri, guid, parentGuid) {
          if (itemType != PlacesUtils.bookmarks.TYPE_BOOKMARK)
            return;

          let keywords = keywordsForHref(uri.spec);
          // This uri has no keywords associated, so there's nothing to do.
          if (keywords.length == 0)
            return;

          (async function() {
            // If the uri is not bookmarked anymore, we can remove this keyword.
            let bookmark = await PlacesUtils.bookmarks.fetch({ url: uri });
            if (!bookmark) {
              for (let keyword of keywords) {
                await PlacesUtils.keywords.remove(keyword);
              }
            }
          })().catch(Cu.reportError);
        },

        onItemChanged(id, prop, isAnno, val, lastMod, itemType, parentId, guid,
                      parentGuid, oldVal) {
          if (gIgnoreKeywordNotifications) {
            return;
          }

          if (prop == "keyword") {
            this._onKeywordChanged(guid, val).catch(Cu.reportError);
          } else if (prop == "uri") {
            this._onUrlChanged(guid, val, oldVal).catch(Cu.reportError);
          }
        },

        async _onKeywordChanged(guid, keyword) {
          let bookmark = await PlacesUtils.bookmarks.fetch(guid);
          // Due to mixed sync/async operations, by this time the bookmark could
          // have disappeared and we already handle removals in onItemRemoved.
          if (!bookmark) {
            return;
          }

          if (keyword.length == 0) {
            // We are removing a keyword.
            let keywords = keywordsForHref(bookmark.url.href)
            for (let kw of keywords) {
              cache.delete(kw);
            }
          } else {
            // We are adding a new keyword.
            cache.set(keyword, { keyword, url: bookmark.url });
          }
        },

        async _onUrlChanged(guid, url, oldUrl) {
          // Check if the old url is associated with keywords.
          let entries = [];
          await PlacesUtils.keywords.fetch({ url: oldUrl }, e => entries.push(e));
          if (entries.length == 0) {
            return;
          }

          // Move the keywords to the new url.
          for (let entry of entries) {
            await PlacesUtils.keywords.remove(entry.keyword);
            entry.url = new URL(url);
            await PlacesUtils.keywords.insert(entry);
          }
        },
      };

      PlacesUtils.bookmarks.addObserver(observer);
      PlacesUtils.registerShutdownFunction(() => {
        PlacesUtils.bookmarks.removeObserver(observer);
      });
      return cache;
    }
));

// Sometime soon, likely as part of the transition to mozIAsyncBookmarks,
// itemIds will be deprecated in favour of GUIDs, which play much better
// with multiple undo/redo operations.  Because these GUIDs are already stored,
// and because we don't want to revise the transactions API once more when this
// happens, transactions are set to work with GUIDs exclusively, in the sense
// that they may never expose itemIds, nor do they accept them as input.
// More importantly, transactions which add or remove items guarantee to
// restore the GUIDs on undo/redo, so that the following transactions that may
// done or undo can assume the items they're interested in are stil accessible
// through the same GUID.
// The current bookmarks API, however, doesn't expose the necessary means for
// working with GUIDs.  So, until it does, this helper object accesses the
// Places database directly in order to switch between GUIDs and itemIds, and
// "restore" GUIDs on items re-created items.
var GuidHelper = {
  // Cache for GUID<->itemId paris.
  guidsForIds: new Map(),
  idsForGuids: new Map(),

  async getItemId(aGuid) {
    let cached = this.idsForGuids.get(aGuid);
    if (cached !== undefined)
      return cached;

    let itemId = await PlacesUtils.withConnectionWrapper("GuidHelper.getItemId",
                                                         async function(db) {
      let rows = await db.executeCached(
        "SELECT b.id, b.guid from moz_bookmarks b WHERE b.guid = :guid LIMIT 1",
        { guid: aGuid });
      if (rows.length == 0)
        throw new Error("no item found for the given GUID");

      return rows[0].getResultByName("id");
    });

    this.updateCache(itemId, aGuid);
    return itemId;
  },

  async getManyItemIds(aGuids) {
    let uncachedGuids = aGuids.filter(guid => !this.idsForGuids.has(guid));
    if (uncachedGuids.length) {
      await PlacesUtils.withConnectionWrapper("GuidHelper.getItemId",
                                              async db => {
        while (uncachedGuids.length) {
          let chunk = uncachedGuids.splice(0, 100);
          let rows = await db.executeCached(
            `SELECT b.id, b.guid from moz_bookmarks b WHERE
             b.guid IN (${"?,".repeat(chunk.length - 1) + "?"})
             LIMIT ${chunk.length}`, chunk);
          if (rows.length < chunk.length)
            throw new Error("Not all items were found!");
          for (let row of rows) {
            this.updateCache(row.getResultByIndex(0), row.getResultByIndex(1));
          }
        }
      });
    }
    return new Map(aGuids.map(guid => [guid, this.idsForGuids.get(guid)]));
  },

  async getItemGuid(aItemId) {
    let cached = this.guidsForIds.get(aItemId);
    if (cached !== undefined)
      return cached;

    let guid = await PlacesUtils.withConnectionWrapper("GuidHelper.getItemGuid",
                                                       async function(db) {

      let rows = await db.executeCached(
        "SELECT b.id, b.guid from moz_bookmarks b WHERE b.id = :id LIMIT 1",
        { id: aItemId });
      if (rows.length == 0)
        throw new Error("no item found for the given itemId");

      return rows[0].getResultByName("guid");
    });

    this.updateCache(aItemId, guid);
    return guid;
  },

  /**
   * Updates the cache.
   *
   * @note This is the only place where the cache should be populated,
   *       invalidation relies on both Maps being populated at the same time.
   */
  updateCache(aItemId, aGuid) {
    if (typeof(aItemId) != "number" || aItemId <= 0)
      throw new Error("Trying to update the GUIDs cache with an invalid itemId");
    if (typeof(aGuid) != "string" || !/^[a-zA-Z0-9\-_]{12}$/.test(aGuid))
      throw new Error("Trying to update the GUIDs cache with an invalid GUID");
    this.ensureObservingRemovedItems();
    this.guidsForIds.set(aItemId, aGuid);
    this.idsForGuids.set(aGuid, aItemId);
  },

  invalidateCacheForItemId(aItemId) {
    let guid = this.guidsForIds.get(aItemId);
    this.guidsForIds.delete(aItemId);
    this.idsForGuids.delete(guid);
  },

  ensureObservingRemovedItems() {
    if (!("observer" in this)) {
      /**
       * This observers serves two purposes:
       * (1) Invalidate cached id<->GUID paris on when items are removed.
       * (2) Cache GUIDs given us free of charge by onItemAdded/onItemRemoved.
      *      So, for exmaple, when the NewBookmark needs the new GUID, we already
      *      have it cached.
      */
      this.observer = {
        onItemAdded: (aItemId, aParentId, aIndex, aItemType, aURI, aTitle,
                      aDateAdded, aGuid, aParentGuid) => {
          this.updateCache(aItemId, aGuid);
          this.updateCache(aParentId, aParentGuid);
        },
        onItemRemoved:
        (aItemId, aParentId, aIndex, aItemTyep, aURI, aGuid, aParentGuid) => {
          this.guidsForIds.delete(aItemId);
          this.idsForGuids.delete(aGuid);
          this.updateCache(aParentId, aParentGuid);
        },

        QueryInterface: XPCOMUtils.generateQI(Ci.nsINavBookmarkObserver),

        onBeginUpdateBatch() {},
        onEndUpdateBatch() {},
        onItemChanged() {},
        onItemVisited() {},
        onItemMoved() {},
      };
      PlacesUtils.bookmarks.addObserver(this.observer);
      PlacesUtils.registerShutdownFunction(() => {
        PlacesUtils.bookmarks.removeObserver(this.observer);
      });
    }
  }
};

// Transactions handlers.

/**
 * Updates commands in the undo group of the active window commands.
 * Inactive windows commands will be updated on focus.
 */
function updateCommandsOnActiveWindow() {
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
function TransactionItemCache() {
}

TransactionItemCache.prototype = {
  set id(v) {
    this._id = (parseInt(v) > 0 ? v : null);
  },
  get id() {
    return this._id || -1;
  },
  set parentId(v) {
    this._parentId = (parseInt(v) > 0 ? v : null);
  },
  get parentId() {
    return this._parentId || -1;
  },
  keyword: null,
  title: null,
  dateAdded: null,
  lastModified: null,
  postData: null,
  itemType: null,
  set uri(v) {
    this._uri = (v instanceof Ci.nsIURI ? v.clone() : null);
  },
  get uri() {
    return this._uri || null;
  },
  set feedURI(v) {
    this._feedURI = (v instanceof Ci.nsIURI ? v.clone() : null);
  },
  get feedURI() {
    return this._feedURI || null;
  },
  set siteURI(v) {
    this._siteURI = (v instanceof Ci.nsIURI ? v.clone() : null);
  },
  get siteURI() {
    return this._siteURI || null;
  },
  set index(v) {
    this._index = (parseInt(v) >= 0 ? v : null);
  },
  // Index can be 0.
  get index() {
    return this._index != null ? this._index : PlacesUtils.bookmarks.DEFAULT_INDEX;
  },
  set annotations(v) {
    this._annotations = Array.isArray(v) ? Cu.cloneInto(v, {}) : null;
  },
  get annotations() {
    return this._annotations || null;
  },
  set tags(v) {
    this._tags = (v && Array.isArray(v) ? Array.prototype.slice.call(v) : null);
  },
  get tags() {
    return this._tags || null;
  },
};


/**
 * Base transaction implementation.
 *
 * @note used internally, DO NOT EXPORT.
 */
function BaseTransaction() {
}

BaseTransaction.prototype = {
  name: null,
  set childTransactions(v) {
    this._childTransactions = (Array.isArray(v) ? Array.prototype.slice.call(v) : null);
  },
  get childTransactions() {
    return this._childTransactions || null;
  },
  doTransaction: function BTXN_doTransaction() {},
  redoTransaction: function BTXN_redoTransaction() {
    return this.doTransaction();
  },
  undoTransaction: function BTXN_undoTransaction() {},
  merge: function BTXN_merge() {
    return false;
  },
  get isTransient() {
    return false;
  },
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
 function PlacesAggregatedTransaction(aName, aTransactions) {
  // Copy the transactions array to decouple it from its prototype, which
  // otherwise keeps alive its associated global object.
  this.childTransactions = aTransactions;
  this.name = aName;
  this.item = new TransactionItemCache();

  // Check child transactions number.  We will batch if we have more than
  // MIN_TRANSACTIONS_FOR_BATCH total number of transactions.
  let countTransactions = function(aTransactions, aTxnCount) {
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

  doTransaction: function ATXN_doTransaction() {
    this._isUndo = false;
    if (this._useBatch)
      PlacesUtils.bookmarks.runInBatchMode(this, null);
    else
      this.runBatched(false);
  },

  undoTransaction: function ATXN_undoTransaction() {
    this._isUndo = true;
    if (this._useBatch)
      PlacesUtils.bookmarks.runInBatchMode(this, null);
    else
      this.runBatched(true);
  },

  runBatched: function ATXN_runBatched() {
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
                                        aChildTransactions) {
  this.item = new TransactionItemCache();
  this.item.title = aTitle;
  this.item.parentId = aParentId;
  this.item.index = aIndex;
  this.item.annotations = aAnnotations;
  this.childTransactions = aChildTransactions;
}

PlacesCreateFolderTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CFTXN_doTransaction() {
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

  undoTransaction: function CFTXN_undoTransaction() {
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
 * @param [optional] aPostData
 *        keyword's POST data, if available.
 *
 * @return nsITransaction object
 */
this.PlacesCreateBookmarkTransaction =
 function PlacesCreateBookmarkTransaction(aURI, aParentId, aIndex, aTitle,
                                          aKeyword, aAnnotations,
                                          aChildTransactions, aPostData) {
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.item.parentId = aParentId;
  this.item.index = aIndex;
  this.item.title = aTitle;
  this.item.keyword = aKeyword;
  this.item.postData = aPostData;
  this.item.annotations = aAnnotations;
  this.childTransactions = aChildTransactions;
}

PlacesCreateBookmarkTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CITXN_doTransaction() {
    this.item.id = PlacesUtils.bookmarks.insertBookmark(this.item.parentId,
                                                        this.item.uri,
                                                        this.item.index,
                                                        this.item.title);
    if (this.item.keyword) {
      PlacesUtils.bookmarks.setKeywordForBookmark(this.item.id,
                                                  this.item.keyword);
      if (this.item.postData) {
        PlacesUtils.setPostDataForBookmark(this.item.id,
                                           this.item.postData);
      }
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

  undoTransaction: function CITXN_undoTransaction() {
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
 function PlacesCreateSeparatorTransaction(aParentId, aIndex) {
  this.item = new TransactionItemCache();
  this.item.parentId = aParentId;
  this.item.index = aIndex;
}

PlacesCreateSeparatorTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function CSTXN_doTransaction() {
    this.item.id =
      PlacesUtils.bookmarks.insertSeparator(this.item.parentId, this.item.index);
  },

  undoTransaction: function CSTXN_undoTransaction() {
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
                                          aIndex, aAnnotations) {
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

  doTransaction: function CLTXN_doTransaction() {
    this._promise = PlacesUtils.livemarks.addLivemark(
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

  undoTransaction: function CLTXN_undoTransaction() {
    // The getLivemark callback may fail, but it is used just to serialize,
    // so it doesn't matter.
    this._promise = PlacesUtils.livemarks.getLivemark({ id: this.item.id })
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
function PlacesRemoveLivemarkTransaction(aLivemarkId) {
  this.item = new TransactionItemCache();
  this.item.id = aLivemarkId;
  this.item.title = PlacesUtils.bookmarks.getItemTitle(this.item.id);
  this.item.parentId = PlacesUtils.bookmarks.getFolderIdForItem(this.item.id);

  let annos = PlacesUtils.getAnnotationsForItem(this.item.id);
  // Exclude livemark service annotations, those will be recreated automatically
  let annosToExclude = [PlacesUtils.LMANNO_FEEDURI,
                        PlacesUtils.LMANNO_SITEURI];
  this.item.annotations = annos.filter(function(aValue, aIndex, aArray) {
      return !annosToExclude.includes(aValue.name);
    });
  this.item.dateAdded = PlacesUtils.bookmarks.getItemDateAdded(this.item.id);
  this.item.lastModified =
    PlacesUtils.bookmarks.getItemLastModified(this.item.id);
}

PlacesRemoveLivemarkTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function RLTXN_doTransaction() {
    PlacesUtils.livemarks.getLivemark({ id: this.item.id })
      .then(aLivemark => {
        this.item.feedURI = aLivemark.feedURI;
        this.item.siteURI = aLivemark.siteURI;
        PlacesUtils.bookmarks.removeItem(this.item.id);
      }, Cu.reportError);
  },

  undoTransaction: function RLTXN_undoTransaction() {
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
 function PlacesMoveItemTransaction(aItemId, aNewParentId, aNewIndex) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.item.parentId = PlacesUtils.bookmarks.getFolderIdForItem(this.item.id);
  this.new = new TransactionItemCache();
  this.new.parentId = aNewParentId;
  this.new.index = aNewIndex;
}

PlacesMoveItemTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function MITXN_doTransaction() {
    this.item.index = PlacesUtils.bookmarks.getItemIndex(this.item.id);
    PlacesUtils.bookmarks.moveItem(this.item.id,
                                   this.new.parentId, this.new.index);
    this._undoIndex = PlacesUtils.bookmarks.getItemIndex(this.item.id);
  },

  undoTransaction: function MITXN_undoTransaction() {
    // moving down in the same parent takes in count removal of the item
    // so to revert positions we must move to oldIndex + 1
    if (this.new.parentId == this.item.parentId &&
        this.item.index > this._undoIndex) {
      PlacesUtils.bookmarks.moveItem(this.item.id, this.item.parentId,
                                     this.item.index + 1);
    } else {
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
 function PlacesRemoveItemTransaction(aItemId) {
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
  } else if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
    this.item.uri = PlacesUtils.bookmarks.getBookmarkURI(this.item.id);
    this.item.keyword =
      PlacesUtils.bookmarks.getKeywordForBookmark(this.item.id);
    if (this.item.keyword)
      this.item.postData = PlacesUtils.getPostDataForBookmark(this.item.id);
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

  doTransaction: function RITXN_doTransaction() {
    this.item.index = PlacesUtils.bookmarks.getItemIndex(this.item.id);

    if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      let txn = new PlacesAggregatedTransaction("Remove item childTxn",
                                                this.childTransactions);
      txn.doTransaction();
    } else {
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

  undoTransaction: function RITXN_undoTransaction() {
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
        if (this.item.postData) {
          PlacesUtils.bookmarks.setPostDataForBookmark(this.item.id);
        }
      }
    } else if (this.item.itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      let txn = new PlacesAggregatedTransaction("Remove item childTxn",
                                                this.childTransactions);
      txn.undoTransaction();
    } else { // TYPE_SEPARATOR
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
  function RITXN__getFolderContentsTransactions() {
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
 function PlacesEditItemTitleTransaction(aItemId, aNewTitle) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.title = aNewTitle;
}

PlacesEditItemTitleTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EITTXN_doTransaction() {
    this.item.title = PlacesUtils.bookmarks.getItemTitle(this.item.id);
    PlacesUtils.bookmarks.setItemTitle(this.item.id, this.new.title);
  },

  undoTransaction: function EITTXN_undoTransaction() {
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

  doTransaction: function EBUTXN_doTransaction() {
    this.item.uri = PlacesUtils.bookmarks.getBookmarkURI(this.item.id);
    PlacesUtils.bookmarks.changeBookmarkURI(this.item.id, this.new.uri);
    // move tags from old URI to new URI
    this.item.tags = PlacesUtils.tagging.getTagsForURI(this.item.uri);
    if (this.item.tags.length > 0) {
      // only untag the old URI if this is the only bookmark
      if (PlacesUtils.getBookmarksForURI(this.item.uri, {}).length == 0)
        PlacesUtils.tagging.untagURI(this.item.uri, this.item.tags);
      PlacesUtils.tagging.tagURI(this.new.uri, this.item.tags);
    }
  },

  undoTransaction: function EBUTXN_undoTransaction() {
    PlacesUtils.bookmarks.changeBookmarkURI(this.item.id, this.item.uri);
    // move tags from new URI to old URI
    if (this.item.tags.length > 0) {
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
 function PlacesSetItemAnnotationTransaction(aItemId, aAnnotationObject) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.annotations = [aAnnotationObject];
}

PlacesSetItemAnnotationTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SIATXN_doTransaction() {
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
                                value,
                                expires: expires.value }];
    } else {
      // create an empty old anno
      this.item.annotations = [{ name: annoName,
                                flags: 0,
                                value: null,
                                expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
    }

    PlacesUtils.setAnnotationsForItem(this.item.id, this.new.annotations);
  },

  undoTransaction: function SIATXN_undoTransaction() {
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
 function PlacesSetPageAnnotationTransaction(aURI, aAnnotationObject) {
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.new = new TransactionItemCache();
  this.new.annotations = [aAnnotationObject];
}

PlacesSetPageAnnotationTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SPATXN_doTransaction() {
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
                                value,
                                expires: expires.value }];
    } else {
      // create an empty old anno
      this.item.annotations = [{ name: annoName,
                                type: Ci.nsIAnnotationService.TYPE_STRING,
                                flags: 0,
                                value: null,
                                expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
    }

    PlacesUtils.setAnnotationsForURI(this.item.uri, this.new.annotations);
  },

  undoTransaction: function SPATXN_undoTransaction() {
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
 * @param aNewPostData [optional]
 *        new keyword's POST data, if available
 * @param aOldKeyword [optional]
 *        old keyword of the bookmark
 *
 * @return nsITransaction object
 */
this.PlacesEditBookmarkKeywordTransaction =
  function PlacesEditBookmarkKeywordTransaction(aItemId, aNewKeyword,
                                                aNewPostData, aOldKeyword) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.item.keyword = aOldKeyword;
  this.item.href = (PlacesUtils.bookmarks.getBookmarkURI(aItemId)).spec;
  this.new = new TransactionItemCache();
  this.new.keyword = aNewKeyword;
  this.new.postData = aNewPostData
}

PlacesEditBookmarkKeywordTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EBKTXN_doTransaction() {
    let done = false;
    (async () => {
      if (this.item.keyword) {
        let oldEntry = await PlacesUtils.keywords.fetch(this.item.keyword);
        this.item.postData = oldEntry.postData;
        await PlacesUtils.keywords.remove(this.item.keyword);
      }

      if (this.new.keyword) {
        await PlacesUtils.keywords.insert({
          url: this.item.href,
          keyword: this.new.keyword,
          postData: this.new.postData || this.item.postData
        });
      }
    })().catch(Cu.reportError)
                 .then(() => done = true);
    // TODO: Until we can move to PlacesTransactions.jsm, we must spin the
    // events loop :(
    let thread = Services.tm.currentThread;
    while (!done) {
      thread.processNextEvent(true);
    }
  },

  undoTransaction: function EBKTXN_undoTransaction() {

    let done = false;
    (async () => {
      if (this.new.keyword) {
        await PlacesUtils.keywords.remove(this.new.keyword);
      }

      if (this.item.keyword) {
        await PlacesUtils.keywords.insert({
          url: this.item.href,
          keyword: this.item.keyword,
          postData: this.item.postData
        });
      }
    })().catch(Cu.reportError)
                 .then(() => done = true);
    // TODO: Until we can move to PlacesTransactions.jsm, we must spin the
    // events loop :(
    let thread = Services.tm.currentThread;
    while (!done) {
      thread.processNextEvent(true);
    }
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
 function PlacesEditBookmarkPostDataTransaction(aItemId, aPostData) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.postData = aPostData;
}

PlacesEditBookmarkPostDataTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction() {
    // Setting null postData is not supported by the current schema.
    if (this.new.postData) {
      this.item.postData = PlacesUtils.getPostDataForBookmark(this.item.id);
      PlacesUtils.setPostDataForBookmark(this.item.id, this.new.postData);
    }
  },

  undoTransaction() {
    // Setting null postData is not supported by the current schema.
    if (this.item.postData) {
      PlacesUtils.setPostDataForBookmark(this.item.id, this.item.postData);
    }
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
 function PlacesEditItemDateAddedTransaction(aItemId, aNewDateAdded) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.dateAdded = aNewDateAdded;
}

PlacesEditItemDateAddedTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function EIDATXN_doTransaction() {
    // Child transactions have the id set as parentId.
    if (this.item.id == -1 && this.item.parentId != -1)
      this.item.id = this.item.parentId;
    this.item.dateAdded =
      PlacesUtils.bookmarks.getItemDateAdded(this.item.id);
    PlacesUtils.bookmarks.setItemDateAdded(this.item.id, this.new.dateAdded);
  },

  undoTransaction: function EIDATXN_undoTransaction() {
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
 function PlacesEditItemLastModifiedTransaction(aItemId, aNewLastModified) {
  this.item = new TransactionItemCache();
  this.item.id = aItemId;
  this.new = new TransactionItemCache();
  this.new.lastModified = aNewLastModified;
}

PlacesEditItemLastModifiedTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction:
  function EILMTXN_doTransaction() {
    // Child transactions have the id set as parentId.
    if (this.item.id == -1 && this.item.parentId != -1)
      this.item.id = this.item.parentId;
    this.item.lastModified =
      PlacesUtils.bookmarks.getItemLastModified(this.item.id);
    PlacesUtils.bookmarks.setItemLastModified(this.item.id,
                                              this.new.lastModified);
  },

  undoTransaction:
  function EILMTXN_undoTransaction() {
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
 function PlacesSortFolderByNameTransaction(aFolderId) {
  this.item = new TransactionItemCache();
  this.item.id = aFolderId;
}

PlacesSortFolderByNameTransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function SFBNTXN_doTransaction() {
    this._oldOrder = [];

    let contents =
      PlacesUtils.getFolderContents(this.item.id, false, false).root;
    let count = contents.childCount;

    // sort between separators
    let newOrder = [];
    let preSep = []; // temporary array for sorting each group of items
    let sortingMethod =
      function(a, b) {
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
      } else
        preSep.push(item);
    }
    contents.containerOpen = false;

    if (preSep.length > 0) {
      preSep.sort(sortingMethod);
      newOrder = newOrder.concat(preSep);
    }

    // set the nex indexes
    let callback = {
      runBatched() {
        for (let i = 0; i < newOrder.length; ++i) {
          PlacesUtils.bookmarks.setItemIndex(newOrder[i].itemId, i);
        }
      }
    };
    PlacesUtils.bookmarks.runInBatchMode(callback, null);
  },

  undoTransaction: function SFBNTXN_undoTransaction() {
    let callback = {
      _self: this,
      runBatched() {
        for (let item in this._self._oldOrder)
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
 function PlacesTagURITransaction(aURI, aTags) {
  this.item = new TransactionItemCache();
  this.item.uri = aURI;
  this.item.tags = aTags;
}

PlacesTagURITransaction.prototype = {
  __proto__: BaseTransaction.prototype,

  doTransaction: function TUTXN_doTransaction() {
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

  undoTransaction: function TUTXN_undoTransaction() {
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
 function PlacesUntagURITransaction(aURI, aTags) {
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

  doTransaction: function UTUTXN_doTransaction() {
    // Filter tags existing on the bookmark, otherwise on undo we may try to
    // set nonexistent tags.
    let tags = PlacesUtils.tagging.getTagsForURI(this.item.uri);
    this.item.tags = this.item.tags.filter(function(aTag) {
      return tags.includes(aTag);
    });
    PlacesUtils.tagging.untagURI(this.item.uri, this.item.tags);
  },

  undoTransaction: function UTUTXN_undoTransaction() {
    PlacesUtils.tagging.tagURI(this.item.uri, this.item.tags);
  }
};

/**
 * Executes a boolean validate function, throwing if it returns false.
 *
 * @param boolValidateFn
 *        A boolean validate function.
 * @return the input value.
 * @throws if input doesn't pass the validate function.
 */
function simpleValidateFunc(boolValidateFn) {
  return (v, input) => {
    if (!boolValidateFn(v, input))
      throw new Error("Invalid value");
    return v;
  };
}
