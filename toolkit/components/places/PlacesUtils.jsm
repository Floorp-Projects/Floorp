/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PlacesUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
  Bookmarks: "resource://gre/modules/Bookmarks.jsm",
  History: "resource://gre/modules/History.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "MOZ_ACTION_REGEX", () => {
  return /^moz-action:([^,]+),(.*)$/;
});

XPCOMUtils.defineLazyGetter(lazy, "gCryptoHash", () => {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where
// we really just want "\n". On other platforms, the transferable system
// converts "\r\n" to "\n".
const NEWLINE = AppConstants.platform == "macosx" ? "\n" : "\r\n";

// Timers resolution is not always good, it can have a 16ms precision on Win.
const TIMERS_RESOLUTION_SKEW_MS = 16;

function QI_node(aNode, aIID) {
  try {
    return aNode.QueryInterface(aIID);
  } catch (ex) {}
  return null;
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
  for (let bookmark of bookmarks) {
    let ids = await PlacesUtils.promiseManyItemIds([
      bookmark.guid,
      bookmark.parentGuid,
    ]);
    bookmark.id = ids.get(bookmark.guid);
    bookmark.parentId = ids.get(bookmark.parentGuid);
  }
  let observers = PlacesUtils.bookmarks.getObservers();
  for (let bookmark of bookmarks) {
    notify(observers, "onItemChanged", [
      bookmark.id,
      "keyword",
      false,
      keyword,
      bookmark.lastModified * 1000,
      bookmark.type,
      bookmark.parentId,
      bookmark.guid,
      bookmark.parentGuid,
      "",
      source,
    ]);
  }
}

/**
 * Serializes the given node in JSON format.
 *
 * @param aNode
 *        An nsINavHistoryResultNode
 */
function serializeNode(aNode) {
  let data = {};

  data.title = aNode.title;
  // The id is no longer used for copying within the same instance/session of
  // Firefox as of at least 61. However, we keep the id for now to maintain
  // backwards compat of drag and drop with older Firefox versions.
  data.id = aNode.itemId;
  data.itemGuid = aNode.bookmarkGuid;
  // Add an instanceId so we can tell which instance of an FF session the data
  // is coming from.
  data.instanceId = PlacesUtils.instanceId;

  let guid = aNode.bookmarkGuid;

  // Some nodes, e.g. the unfiled/menu/toolbar ones can have a virtual guid, so
  // we ignore any that are a folder shortcut. These will be handled below.
  if (
    guid &&
    !PlacesUtils.bookmarks.isVirtualRootItem(guid) &&
    !PlacesUtils.isVirtualLeftPaneItem(guid)
  ) {
    if (aNode.parent) {
      data.parent = aNode.parent.itemId;
      data.parentGuid = aNode.parent.bookmarkGuid;
    }

    data.dateAdded = aNode.dateAdded;
    data.lastModified = aNode.lastModified;
  }

  if (PlacesUtils.nodeIsURI(aNode)) {
    // Check for url validity.
    new URL(aNode.uri);
    data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
    data.uri = aNode.uri;
    if (aNode.tags) {
      data.tags = aNode.tags;
    }
  } else if (PlacesUtils.nodeIsFolder(aNode)) {
    if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT) {
      data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      data.uri = aNode.uri;
      data.concreteId = PlacesUtils.getConcreteItemId(aNode);
      data.concreteGuid = PlacesUtils.getConcreteItemGuid(aNode);
    } else {
      data.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
    }
  } else if (PlacesUtils.nodeIsQuery(aNode)) {
    data.type = PlacesUtils.TYPE_X_MOZ_PLACE;
    data.uri = aNode.uri;
  } else if (PlacesUtils.nodeIsSeparator(aNode)) {
    data.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
  }

  return JSON.stringify(data);
}

// Imposed to limit database size.
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;
const DB_DESCRIPTION_LENGTH_MAX = 256;
const DB_SITENAME_LENGTH_MAX = 50;

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
    if (!boolValidateFn(v, input)) {
      throw new Error("Invalid value");
    }
    return v;
  };
}

/**
 * List of bookmark object validators, one per each known property.
 * Validators must throw if the property value is invalid and return a fixed up
 * version of the value, if needed.
 */
const BOOKMARK_VALIDATORS = Object.freeze({
  guid: simpleValidateFunc(v => PlacesUtils.isValidGuid(v)),
  parentGuid: simpleValidateFunc(v => PlacesUtils.isValidGuid(v)),
  guidPrefix: simpleValidateFunc(v => PlacesUtils.isValidGuidPrefix(v)),
  index: simpleValidateFunc(
    v => Number.isInteger(v) && v >= PlacesUtils.bookmarks.DEFAULT_INDEX
  ),
  dateAdded: simpleValidateFunc(v => v.constructor.name == "Date"),
  lastModified: simpleValidateFunc(v => v.constructor.name == "Date"),
  type: simpleValidateFunc(
    v =>
      Number.isInteger(v) &&
      [
        PlacesUtils.bookmarks.TYPE_BOOKMARK,
        PlacesUtils.bookmarks.TYPE_FOLDER,
        PlacesUtils.bookmarks.TYPE_SEPARATOR,
      ].includes(v)
  ),
  title: v => {
    if (v === null) {
      return "";
    }
    if (typeof v == "string") {
      return v.slice(0, DB_TITLE_LENGTH_MAX);
    }
    throw new Error("Invalid title");
  },
  url: v => {
    simpleValidateFunc(
      val =>
        (typeof val == "string" && val.length <= DB_URL_LENGTH_MAX) ||
        (val instanceof Ci.nsIURI && val.spec.length <= DB_URL_LENGTH_MAX) ||
        (URL.isInstance(val) && val.href.length <= DB_URL_LENGTH_MAX)
    )(v);
    if (typeof v === "string") {
      return new URL(v);
    }
    if (v instanceof Ci.nsIURI) {
      return new URL(v.spec);
    }
    return v;
  },
  source: simpleValidateFunc(
    v =>
      Number.isInteger(v) &&
      Object.values(PlacesUtils.bookmarks.SOURCES).includes(v)
  ),
  keyword: simpleValidateFunc(v => typeof v == "string" && v.length),
  charset: simpleValidateFunc(v => typeof v == "string" && v.length),
  postData: simpleValidateFunc(v => typeof v == "string" && v.length),
  tags: simpleValidateFunc(
    v =>
      Array.isArray(v) &&
      v.length &&
      v.every(item => item && typeof item == "string")
  ),
});

// Sync bookmark records can contain additional properties.
const SYNC_BOOKMARK_VALIDATORS = Object.freeze({
  // Sync uses Places GUIDs for all records except roots.
  recordId: simpleValidateFunc(
    v =>
      typeof v == "string" &&
      (lazy.PlacesSyncUtils.bookmarks.ROOTS.includes(v) ||
        PlacesUtils.isValidGuid(v))
  ),
  parentRecordId: v => SYNC_BOOKMARK_VALIDATORS.recordId(v),
  // Sync uses kinds instead of types.
  kind: simpleValidateFunc(
    v =>
      typeof v == "string" &&
      Object.values(lazy.PlacesSyncUtils.bookmarks.KINDS).includes(v)
  ),
  query: simpleValidateFunc(v => v === null || (typeof v == "string" && v)),
  folder: simpleValidateFunc(
    v =>
      typeof v == "string" &&
      v &&
      v.length <= PlacesUtils.bookmarks.MAX_TAG_LENGTH
  ),
  tags: v => {
    if (v === null) {
      return [];
    }
    if (!Array.isArray(v)) {
      throw new Error("Invalid tag array");
    }
    for (let tag of v) {
      if (
        typeof tag != "string" ||
        !tag ||
        tag.length > PlacesUtils.bookmarks.MAX_TAG_LENGTH
      ) {
        throw new Error(`Invalid tag: ${tag}`);
      }
    }
    return v;
  },
  keyword: simpleValidateFunc(v => v === null || typeof v == "string"),
  dateAdded: simpleValidateFunc(
    v =>
      typeof v === "number" &&
      v > lazy.PlacesSyncUtils.bookmarks.EARLIEST_BOOKMARK_TIMESTAMP
  ),
  feed: v => (v === null ? v : BOOKMARK_VALIDATORS.url(v)),
  site: v => (v === null ? v : BOOKMARK_VALIDATORS.url(v)),
  title: BOOKMARK_VALIDATORS.title,
  url: BOOKMARK_VALIDATORS.url,
});

// Sync change records are passed between `PlacesSyncUtils` and the Sync
// bookmarks engine, and are used to update an item's sync status and change
// counter at the end of a sync.
const SYNC_CHANGE_RECORD_VALIDATORS = Object.freeze({
  modified: simpleValidateFunc(v => typeof v == "number" && v >= 0),
  counter: simpleValidateFunc(v => typeof v == "number" && v >= 0),
  status: simpleValidateFunc(
    v =>
      typeof v == "number" &&
      Object.values(PlacesUtils.bookmarks.SYNC_STATUS).includes(v)
  ),
  tombstone: simpleValidateFunc(v => v === true || v === false),
  synced: simpleValidateFunc(v => v === true || v === false),
});
/**
 * List PageInfo bookmark object validators.
 */
const PAGEINFO_VALIDATORS = Object.freeze({
  guid: BOOKMARK_VALIDATORS.guid,
  url: BOOKMARK_VALIDATORS.url,
  title: v => {
    if (v == null || v == undefined) {
      return undefined;
    } else if (typeof v === "string") {
      return v;
    }
    throw new TypeError(
      `title property of PageInfo object: ${v} must be a string if provided`
    );
  },
  previewImageURL: v => {
    if (!v) {
      return null;
    }
    return BOOKMARK_VALIDATORS.url(v);
  },
  description: v => {
    if (typeof v === "string" || v === null) {
      return v ? v.slice(0, DB_DESCRIPTION_LENGTH_MAX) : null;
    }
    throw new TypeError(
      `description property of pageInfo object: ${v} must be either a string or null if provided`
    );
  },
  siteName: v => {
    if (typeof v === "string" || v === null) {
      return v ? v.slice(0, DB_SITENAME_LENGTH_MAX) : null;
    }
    throw new TypeError(
      `siteName property of pageInfo object: ${v} must be either a string or null if provided`
    );
  },
  annotations: v => {
    if (typeof v != "object" || v.constructor.name != "Map") {
      throw new TypeError("annotations must be a Map");
    }

    if (v.size == 0) {
      throw new TypeError("there must be at least one annotation");
    }

    for (let [key, value] of v.entries()) {
      if (typeof key != "string") {
        throw new TypeError("all annotation keys must be strings");
      }
      if (
        typeof value != "string" &&
        typeof value != "number" &&
        typeof value != "boolean" &&
        value !== null &&
        value !== undefined
      ) {
        throw new TypeError(
          "all annotation values must be Boolean, Numbers or Strings"
        );
      }
    }
    return v;
  },
  visits: v => {
    if (!Array.isArray(v) || !v.length) {
      throw new TypeError("PageInfo object must have an array of visits");
    }
    let visits = [];
    for (let inVisit of v) {
      let visit = {
        date: new Date(),
        transition: inVisit.transition || lazy.History.TRANSITIONS.LINK,
      };

      if (!PlacesUtils.history.isValidTransition(visit.transition)) {
        throw new TypeError(
          `transition: ${visit.transition} is not a valid transition type`
        );
      }

      if (inVisit.date) {
        PlacesUtils.history.ensureDate(inVisit.date);
        if (inVisit.date > Date.now() + TIMERS_RESOLUTION_SKEW_MS) {
          throw new TypeError(`date: ${inVisit.date} cannot be a future date`);
        }
        visit.date = inVisit.date;
      }

      if (inVisit.referrer) {
        visit.referrer = PlacesUtils.normalizeToURLOrGUID(inVisit.referrer);
      }
      visits.push(visit);
    }
    return visits;
  },
});

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
  // Used to track the action that populated the clipboard.
  TYPE_X_MOZ_PLACE_ACTION: "text/x-moz-place-action",

  // Deprecated: Remaining only for supporting migration of old livemarks.
  LMANNO_FEEDURI: "livemark/feedURI",
  LMANNO_SITEURI: "livemark/siteURI",
  CHARSET_ANNO: "URIProperties/characterSet",
  // Deprecated: This is only used for supporting import from older datasets.
  MOBILE_ROOT_ANNO: "mobile/bookmarksRoot",

  TOPIC_SHUTDOWN: "places-shutdown",
  TOPIC_INIT_COMPLETE: "places-init-complete",
  TOPIC_DATABASE_LOCKED: "places-database-locked",
  TOPIC_EXPIRATION_FINISHED: "places-expiration-finished",
  TOPIC_FAVICONS_EXPIRED: "places-favicons-expired",
  TOPIC_VACUUM_STARTING: "places-vacuum-starting",
  TOPIC_BOOKMARKS_RESTORE_BEGIN: "bookmarks-restore-begin",
  TOPIC_BOOKMARKS_RESTORE_SUCCESS: "bookmarks-restore-success",
  TOPIC_BOOKMARKS_RESTORE_FAILED: "bookmarks-restore-failed",

  observers: PlacesObservers,

  /**
   * GUIDs associated with virtual queries that are used for displaying the
   * top-level folders in the left pane.
   */
  virtualAllBookmarksGuid: "allbms_____v",
  virtualHistoryGuid: "history____v",
  virtualDownloadsGuid: "downloads__v",
  virtualTagsGuid: "tags_______v",

  /**
   * Checks if a guid is a virtual left-pane root.
   *
   * @param {String} guid The guid of the item to look for.
   * @returns {Boolean} true if guid is a virtual root, false otherwise.
   */
  isVirtualLeftPaneItem(guid) {
    return (
      guid == PlacesUtils.virtualAllBookmarksGuid ||
      guid == PlacesUtils.virtualHistoryGuid ||
      guid == PlacesUtils.virtualDownloadsGuid ||
      guid == PlacesUtils.virtualTagsGuid
    );
  },

  asContainer: aNode => asContainer(aNode),
  asQuery: aNode => asQuery(aNode),

  endl: NEWLINE,

  /**
   * Is a string a valid GUID?
   *
   * @param guid: (String)
   * @return (Boolean)
   */
  isValidGuid(guid) {
    return typeof guid == "string" && guid && /^[a-zA-Z0-9\-_]{12}$/.test(guid);
  },

  /**
   * Is a string a valid GUID prefix?
   *
   * @param guidPrefix: (String)
   * @return (Boolean)
   */
  isValidGuidPrefix(guidPrefix) {
    return (
      typeof guidPrefix == "string" &&
      guidPrefix &&
      /^[a-zA-Z0-9\-_]{1,11}$/.test(guidPrefix)
    );
  },

  /**
   * Generates a random GUID and replace its beginning with the given
   * prefix. We do this instead of just prepending the prefix to keep
   * the correct character length.
   *
   * @param prefix: (String)
   * @return (String)
   */
  generateGuidWithPrefix(prefix) {
    return prefix + this.history.makeGuid().substring(prefix.length);
  },

  /**
   * Converts a string or n URL object to an nsIURI.
   *
   * @param url (URL) or (String)
   *        the URL to convert.
   * @return nsIURI for the given URL.
   */
  toURI(url) {
    url = URL.isInstance(url) ? url.href : url;

    return lazy.NetUtil.newURI(url);
  },

  /**
   * Convert a Date object to a PRTime (microseconds).
   *
   * @param date
   *        the Date object to convert.
   * @return microseconds from the epoch.
   */
  toPRTime(date) {
    if (typeof date != "number" && date.constructor.name != "Date") {
      throw new Error("Invalid value passed to toPRTime");
    }
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
    if (typeof time != "number") {
      throw new Error("Invalid value passed to toDate");
    }
    return new Date(parseInt(time / 1000));
  },

  /**
   * Wraps a string in a nsISupportsString wrapper.
   * @param   aString
   *          The string to wrap.
   * @returns A nsISupportsString object containing a string.
   */
  toISupportsString: function PU_toISupportsString(aString) {
    let s = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    s.data = aString;
    return s;
  },

  getFormattedString: function PU_getFormattedString(key, params) {
    return lazy.bundle.formatStringFromName(key, params);
  },

  getString: function PU_getString(key) {
    return lazy.bundle.GetStringFromName(key);
  },

  /**
   * Parses a moz-action URL and returns its parts.
   *
   * @param url A moz-action URI.
   * @note URL is in the format moz-action:ACTION,JSON_ENCODED_PARAMS
   */
  parseActionUrl(url) {
    if (url instanceof Ci.nsIURI) {
      url = url.spec;
    } else if (URL.isInstance(url)) {
      url = url.href;
    }
    // Faster bailout.
    if (!url.startsWith("moz-action:")) {
      return null;
    }

    try {
      let [, type, params] = url.match(lazy.MOZ_ACTION_REGEX);
      let action = {
        type,
        params: JSON.parse(params),
      };
      for (let key in action.params) {
        action.params[key] = decodeURIComponent(action.params[key]);
      }
      return action;
    } catch (ex) {
      Cu.reportError(`Invalid action url "${url}"`);
      return null;
    }
  },

  /**
   * Determines if a folder is generated from a query.
   * @param aNode a result true.
   * @returns true if the node is a folder generated from a query.
   */
  isQueryGeneratedFolder(node) {
    if (!node.parent) {
      return false;
    }
    return this.nodeIsFolder(node) && this.nodeIsQuery(node.parent);
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark folder.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PU_nodeIsFolder(aNode) {
    return (
      aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER ||
      aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT
    );
  },

  /**
   * Determines whether or not a ResultNode represents a bookmarked URI.
   * @param   aNode
   *          A result node
   * @returns true if the node represents a bookmarked URI, false otherwise
   */
  nodeIsBookmark: function PU_nodeIsBookmark(aNode) {
    return (
      aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_URI &&
      aNode.itemId != -1
    );
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
   * @param {string} name
   *        The operation name. This is included in the error message if
   *        validation fails.
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
   *         - fixup: a function invoked when validation fails, takes the input
   *                  object as argument and must fix the property.
   *
   * @return a validated and normalized item.
   * @throws if the object contains invalid data.
   * @note any unknown properties are pass-through.
   */
  validateItemProperties(name, validators, props, behavior = {}) {
    if (typeof props != "object" || !props) {
      throw new Error(`${name}: Input should be a valid object`);
    }
    // Make a shallow copy of `props` to avoid mutating the original object
    // when filling in defaults.
    let input = Object.assign({}, props);
    let normalizedInput = {};
    let required = new Set();
    for (let prop in behavior) {
      if (
        behavior[prop].hasOwnProperty("required") &&
        behavior[prop].required
      ) {
        required.add(prop);
      }
      if (
        behavior[prop].hasOwnProperty("requiredIf") &&
        behavior[prop].requiredIf(input)
      ) {
        required.add(prop);
      }
      if (
        behavior[prop].hasOwnProperty("validIf") &&
        input[prop] !== undefined &&
        !behavior[prop].validIf(input)
      ) {
        if (behavior[prop].hasOwnProperty("fixup")) {
          behavior[prop].fixup(input);
        } else {
          throw new Error(
            `${name}: Invalid value for property '${prop}': ${JSON.stringify(
              input[prop]
            )}`
          );
        }
      }
      if (
        behavior[prop].hasOwnProperty("defaultValue") &&
        input[prop] === undefined
      ) {
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
          if (
            behavior.hasOwnProperty(prop) &&
            behavior[prop].hasOwnProperty("fixup")
          ) {
            behavior[prop].fixup(input);
            normalizedInput[prop] = input[prop];
          } else {
            throw new Error(
              `${name}: Invalid value for property '${prop}': ${JSON.stringify(
                input[prop]
              )}`
            );
          }
        }
      }
    }
    if (required.size > 0) {
      throw new Error(
        `${name}: The following properties were expected: ${[...required].join(
          ", "
        )}`
      );
    }
    return normalizedInput;
  },

  BOOKMARK_VALIDATORS,
  PAGEINFO_VALIDATORS,
  SYNC_BOOKMARK_VALIDATORS,
  SYNC_CHANGE_RECORD_VALIDATORS,

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  _shutdownFunctions: [],
  registerShutdownFunction: function PU_registerShutdownFunction(aFunc) {
    // If this is the first registered function, add the shutdown observer.
    if (!this._shutdownFunctions.length) {
      Services.obs.addObserver(this, this.TOPIC_SHUTDOWN);
    }
    this._shutdownFunctions.push(aFunc);
  },

  // nsIObserver
  observe: function PU_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case this.TOPIC_SHUTDOWN:
        Services.obs.removeObserver(this, this.TOPIC_SHUTDOWN);
        while (this._shutdownFunctions.length) {
          this._shutdownFunctions.shift().apply(this);
        }
        break;
    }
  },

  /**
   * Determines whether or not a ResultNode is a host container.
   * @param   aNode
   *          A result node
   * @returns true if the node is a host container, false otherwise
   */
  nodeIsHost: function PU_nodeIsHost(aNode) {
    return (
      aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY &&
      aNode.parent &&
      asQuery(aNode.parent).queryOptions.resultType ==
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY
    );
  },

  /**
   * Determines whether or not a ResultNode is a day container.
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a day container, false otherwise
   */
  nodeIsDay: function PU_nodeIsDay(aNode) {
    var resultType;
    return (
      aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY &&
      aNode.parent &&
      ((resultType = asQuery(aNode.parent).queryOptions.resultType) ==
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
        resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY)
    );
  },

  /**
   * Determines whether or not a result-node is a tag container.
   * @param   aNode
   *          A result-node
   * @returns true if the node is a tag container, false otherwise
   */
  nodeIsTagQuery: function PU_nodeIsTagQuery(aNode) {
    if (aNode.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY) {
      return false;
    }
    // Direct child of RESULTS_AS_TAGS_ROOT.
    let parent = aNode.parent;
    if (
      parent &&
      PlacesUtils.asQuery(parent).queryOptions.resultType ==
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAGS_ROOT
    ) {
      return true;
    }
    // We must also support the right pane of the Library, when the tag query
    // is the root node. Unfortunately this is also valid for any tag query
    // selected in the left pane that is not a direct child of RESULTS_AS_TAGS_ROOT.
    if (
      !parent &&
      aNode == aNode.parentResult.root &&
      PlacesUtils.asQuery(aNode).query.tags.length == 1
    ) {
      return true;
    }
    return false;
  },

  /**
   * Determines whether or not a ResultNode is a container.
   * @param   aNode
   *          A result node
   * @returns true if the node is a container item, false otherwise
   */
  containerTypes: [
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
  ],
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
    return (
      this.nodeIsQuery(aNode) &&
      ((resultType = asQuery(aNode).queryOptions.resultType) ==
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY ||
        resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
        resultType == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY ||
        this.nodeIsDay(aNode) ||
        this.nodeIsHost(aNode))
    );
  },

  /**
   * Gets the concrete item-id for the given node. Generally, this is just
   * node.itemId, but for folder-shortcuts that's node.folderItemId.
   */
  getConcreteItemId: function PU_getConcreteItemId(aNode) {
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT
      ? asQuery(aNode).folderItemId
      : aNode.itemId;
  },

  /**
   * Gets the concrete item-guid for the given node. For everything but folder
   * shortcuts, this is just node.bookmarkGuid.  For folder shortcuts, this is
   * node.targetFolderGuid (see nsINavHistoryService.idl for the semantics).
   *
   * @param aNode
   *        a result node.
   * @return the concrete item-guid for aNode.
   */
  getConcreteItemGuid(aNode) {
    if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT) {
      return asQuery(aNode).targetFolderGuid;
    }
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
    return (
      url.host
        .split("")
        .reverse()
        .join("") + "."
    );
  },

  /**
   * String-wraps a result node according to the rules of the specified
   * content type for copy or move operations.
   *
   * @param   aNode
   *          The Result node to wrap (serialize)
   * @param   aType
   *          The content type to serialize as
   * @return  A string serialization of the node
   */
  wrapNode(aNode, aType) {
    // when wrapping a node, we want all the items, even if the original
    // query options are excluding them.
    // This can happen when copying from the left hand pane of the bookmarks
    // organizer.
    // @return [node, shouldClose]
    function gatherDataFromNode(node, gatherDataFunc) {
      if (
        PlacesUtils.nodeIsFolder(node) &&
        node.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT &&
        asQuery(node).queryOptions.excludeItems
      ) {
        let folderRoot = PlacesUtils.getFolderContents(
          node.bookmarkGuid,
          false,
          true
        ).root;
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
      let htmlEscape = s =>
        s
          .replace(/&/g, "&amp;")
          .replace(/>/g, "&gt;")
          .replace(/</g, "&lt;")
          .replace(/"/g, "&quot;")
          .replace(/'/g, "&apos;");

      // escape out potential HTML in the title
      let escapedTitle = node.title ? htmlEscape(node.title) : "";

      if (PlacesUtils.nodeIsContainer(node)) {
        asContainer(node);
        let wasOpen = node.containerOpen;
        if (!wasOpen) {
          node.containerOpen = true;
        }

        let childString = "<DL><DT>" + escapedTitle + "</DT>" + NEWLINE;
        let cc = node.childCount;
        for (let i = 0; i < cc; ++i) {
          childString +=
            "<DD>" +
            NEWLINE +
            gatherDataHtml(node.getChild(i)) +
            "</DD>" +
            NEWLINE;
        }
        node.containerOpen = wasOpen;
        return childString + "</DL>" + NEWLINE;
      }
      if (PlacesUtils.nodeIsURI(node)) {
        return `<A HREF="${node.uri}">${escapedTitle}</A>${NEWLINE}`;
      }
      if (PlacesUtils.nodeIsSeparator(node)) {
        return "<HR>" + NEWLINE;
      }
      return "";
    }

    function gatherDataText(node) {
      if (PlacesUtils.nodeIsContainer(node)) {
        asContainer(node);
        let wasOpen = node.containerOpen;
        if (!wasOpen) {
          node.containerOpen = true;
        }

        let childString = node.title + NEWLINE;
        let cc = node.childCount;
        for (let i = 0; i < cc; ++i) {
          let child = node.getChild(i);
          let suffix = i < cc - 1 ? NEWLINE : "";
          childString += gatherDataText(child) + suffix;
        }
        node.containerOpen = wasOpen;
        return childString;
      }
      if (PlacesUtils.nodeIsURI(node)) {
        return node.uri;
      }
      if (PlacesUtils.nodeIsSeparator(node)) {
        return "--------------------";
      }
      return "";
    }

    switch (aType) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER: {
        // Serialize the node to JSON.
        return serializeNode(aNode);
      }
      case this.TYPE_X_MOZ_URL: {
        if (PlacesUtils.nodeIsURI(aNode)) {
          return aNode.uri + NEWLINE + aNode.title;
        }
        if (PlacesUtils.nodeIsContainer(aNode)) {
          return PlacesUtils.getURLsForContainerNode(aNode)
            .map(item => item.uri + "\n" + item.title)
            .join("\n");
        }
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
   * @throws if the blob contains invalid data.
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
        if (parts.length != 1 && parts.length % 2) {
          break;
        }
        for (let i = 0; i < parts.length; i = i + 2) {
          let uriString = parts[i];
          let titleString = "";
          if (parts.length > i + 1) {
            titleString = parts[i + 1];
          } else {
            // for drag and drop of files, try to use the leafName as title
            try {
              titleString = Services.io
                .newURI(uriString)
                .QueryInterface(Ci.nsIURL).fileName;
            } catch (ex) {}
          }
          // note:  Services.io.newURI() will throw if uriString is not a valid URI
          let uri = Services.io.newURI(uriString);
          if (Services.io.newURI(uriString) && uri.scheme != "place") {
            nodes.push({
              uri: uriString,
              title: titleString ? titleString : uriString,
              type: this.TYPE_X_MOZ_URL,
            });
          }
        }
        break;
      }
      case this.TYPE_UNICODE: {
        let parts = blob.split("\n");
        for (let i = 0; i < parts.length; i++) {
          let uriString = parts[i];
          // text/uri-list is converted to TYPE_UNICODE but it could contain
          // comments line prepended by #, we should skip them, as well as
          // empty uris.
          if (uriString.substr(0, 1) == "\x23" || uriString == "") {
            continue;
          }
          // note: Services.io.newURI) will throw if uriString is not a valid URI
          let uri = Services.io.newURI(uriString);
          if (uri.scheme != "place") {
            nodes.push({
              uri: uriString,
              title: uriString,
              type: this.TYPE_X_MOZ_URL,
            });
          }
        }
        break;
      }
      default:
        throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
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
    return this.validateItemProperties(
      "PageInfo",
      PAGEINFO_VALIDATORS,
      pageInfo,
      {
        url: { requiredIf: b => !b.guid },
        guid: { requiredIf: b => !b.url },
        visits: { requiredIf: b => validateVisits },
      }
    );
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
    if (URL.isInstance(key)) {
      return key;
    }
    if (key instanceof Ci.nsIURI) {
      return new URL(key.spec);
    }
    throw new TypeError("Invalid url or guid: " + key);
  },

  /**
   * Generates a nsINavHistoryResult for the contents of a folder.
   * @param   aFolderGuid
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
  getFolderContents(aFolderGuid, aExcludeItems, aExpandQueries) {
    if (!this.isValidGuid(aFolderGuid)) {
      throw new Error("aFolderGuid should be a valid GUID.");
    }
    var query = this.history.getNewQuery();
    query.setParents([aFolderGuid]);
    var options = this.history.getNewQueryOptions();
    options.excludeItems = aExcludeItems;
    options.expandQueries = aExpandQueries;

    var result = this.history.executeQuery(query, options);
    result.root.containerOpen = true;
    return result;
  },

  // Identifier getters for special folders.
  // You should use these everywhere PlacesUtils is available to avoid XPCOM
  // traversal just to get roots' ids.
  get placesRootId() {
    delete this.placesRootId;
    return (this.placesRootId = this.bookmarks.placesRoot);
  },

  get bookmarksMenuFolderId() {
    delete this.bookmarksMenuFolderId;
    return (this.bookmarksMenuFolderId = this.bookmarks.bookmarksMenuFolder);
  },

  get toolbarFolderId() {
    delete this.toolbarFolderId;
    return (this.toolbarFolderId = this.bookmarks.toolbarFolder);
  },

  get tagsFolderId() {
    delete this.tagsFolderId;
    return (this.tagsFolderId = this.bookmarks.tagsFolder);
  },

  /**
   * Checks if item is a root.
   *
   * @param {String} guid The guid of the item to look for.
   * @returns {Boolean} true if guid is a root, false otherwise.
   */
  isRootItem(guid) {
    return (
      guid == PlacesUtils.bookmarks.menuGuid ||
      guid == PlacesUtils.bookmarks.toolbarGuid ||
      guid == PlacesUtils.bookmarks.unfiledGuid ||
      guid == PlacesUtils.bookmarks.tagsGuid ||
      guid == PlacesUtils.bookmarks.rootGuid ||
      guid == PlacesUtils.bookmarks.mobileGuid
    );
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
  getContainerNodeWithOptions: function PU_getContainerNodeWithOptions(
    aNode,
    aExcludeItems,
    aExpandQueries
  ) {
    if (!this.nodeIsContainer(aNode)) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    // excludeItems is inherited by child containers in an excludeItems view.
    var excludeItems =
      asQuery(aNode).queryOptions.excludeItems ||
      asQuery(aNode.parentResult.root).queryOptions.excludeItems;
    // expandQueries is inherited by child containers in an expandQueries view.
    var expandQueries =
      asQuery(aNode).queryOptions.expandQueries &&
      asQuery(aNode.parentResult.root).queryOptions.expandQueries;

    // If our options are exactly what we expect, directly return the node.
    if (excludeItems == aExcludeItems && expandQueries == aExpandQueries) {
      return aNode;
    }

    // Otherwise, get contents manually.
    var query = {},
      options = {};
    this.history.queryStringToQuery(aNode.uri, query, options);
    options.value.excludeItems = aExcludeItems;
    options.value.expandQueries = aExpandQueries;
    return this.history.executeQuery(query.value, options.value).root;
  },

  /**
   * Returns true if a container has uri nodes in its first level.
   * Has better performance than (getURLsForContainerNode(node).length > 0).
   * @param aNode
   *        The container node to search through.
   * @returns true if the node contains uri nodes, false otherwise.
   */
  hasChildURIs: function PU_hasChildURIs(aNode) {
    if (!this.nodeIsContainer(aNode)) {
      return false;
    }

    let root = this.getContainerNodeWithOptions(aNode, false, true);
    let result = root.parentResult;
    let didSuppressNotifications = false;
    let wasOpen = root.containerOpen;
    if (!wasOpen) {
      didSuppressNotifications = result.suppressNotifications;
      if (!didSuppressNotifications) {
        result.suppressNotifications = true;
      }

      root.containerOpen = true;
    }

    let found = false;
    for (let i = 0; i < root.childCount && !found; i++) {
      let child = root.getChild(i);
      if (this.nodeIsURI(child)) {
        found = true;
      }
    }

    if (!wasOpen) {
      root.containerOpen = false;
      if (!didSuppressNotifications) {
        result.suppressNotifications = false;
      }
    }
    return found;
  },

  getChildCountForFolder(guid) {
    let folder = PlacesUtils.getFolderContents(guid).root;
    let childCount = folder.childCount;
    folder.containerOpen = false;
    return childCount;
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
    if (!this.nodeIsContainer(aNode)) {
      return urls;
    }

    let root = this.getContainerNodeWithOptions(aNode, false, true);
    let result = root.parentResult;
    let wasOpen = root.containerOpen;
    let didSuppressNotifications = false;
    if (!wasOpen) {
      didSuppressNotifications = result.suppressNotifications;
      if (!didSuppressNotifications) {
        result.suppressNotifications = true;
      }

      root.containerOpen = true;
    }

    for (let i = 0; i < root.childCount; ++i) {
      let child = root.getChild(i);
      if (this.nodeIsURI(child)) {
        urls.push({
          uri: child.uri,
          isBookmark: this.nodeIsBookmark(child),
          title: child.title,
        });
      }
    }

    if (!wasOpen) {
      root.containerOpen = false;
      if (!didSuppressNotifications) {
        result.suppressNotifications = false;
      }
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
  promiseDBConnection: () => lazy.gAsyncDBConnPromised,

  /**
   * This is pretty much the same as promiseDBConnection, but with a larger
   * page cache, useful for consumers doing large table scans, like the urlbar.
   * @see promiseDBConnection
   */
  promiseLargeCacheDBConnection: () => lazy.gAsyncDBLargeCacheConnPromised,
  get largeCacheDBConnDeferred() {
    return gAsyncDBLargeCacheConnDeferred;
  },

  /**
   * Returns a Sqlite.jsm wrapper for the main Places connection. Most callers
   * should prefer `withConnectionWrapper`, which ensures that all database
   * operations finish before the connection is closed.
   */
  promiseUnsafeWritableDBConnection: () => lazy.gAsyncDBWrapperPromised,

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
  async withConnectionWrapper(name, task) {
    if (!name) {
      throw new TypeError("Expecting a user-readable name");
    }
    let db = await lazy.gAsyncDBWrapperPromised;
    return db.executeBeforeShutdown(name, task);
  },

  /**
   * Gets favicon data for a given page url.
   *
   * @param {string | URL | nsIURI} aPageUrl
   *   url of the page to look favicon for.
   * @param {number} preferredWidth
   *   The preferred width of the favicon in pixels. The default value of 0
   *   returns the largest icon available.
   * @resolves to an object representing a favicon entry, having the following
   *           properties: { uri, dataLen, data, mimeType }
   * @rejects JavaScript exception if the given url has no associated favicon.
   */
  promiseFaviconData(aPageUrl, preferredWidth = 0) {
    return new Promise((resolve, reject) => {
      if (!(aPageUrl instanceof Ci.nsIURI)) {
        aPageUrl = PlacesUtils.toURI(aPageUrl);
      }
      PlacesUtils.favicons.getFaviconDataForPage(
        aPageUrl,
        function(uri, dataLen, data, mimeType, size) {
          if (uri) {
            resolve({ uri, dataLen, data, mimeType, size });
          } else {
            reject();
          }
        },
        preferredWidth
      );
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
    return (
      href +
      (href.includes("#") ? "&" : "#") +
      "size=" +
      Math.round(size) * window.devicePixelRatio
    );
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
    return GuidHelper.getItemGuid(aItemId);
  },

  /**
   * Get the item id for an item (a bookmark, a folder or a separator) given
   * its unique id.
   *
   * @param aGuid
   *        an item GUID
   * @return {Promise}
   * @resolves to the item id.
   * @rejects if there's no item for the given GUID.
   */
  promiseItemId(aGuid) {
    return GuidHelper.getItemId(aGuid);
  },

  /**
   * Get the item ids for multiple items (a bookmark, a folder or a separator)
   * given the unique ids for each item.
   *
   * @param {Array} aGuids An array of item GUIDs.
   * @return {Promise}
   * @resolves to a Map of item ids.
   * @rejects if not all of the GUIDs could be found.
   */
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
    GuidHelper.invalidateCacheForItemId(aItemId);
  },

  /**
   * Invalidates the entire GUID cache.
   */
  invalidateCachedGuids() {
    GuidHelper.invalidateCache();
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
   *  - typeCode (number):  the item's type in numeric format.
   *    @see PlacesUtils.bookmarks.TYPE_*
   *  - title (string): the item's title. If it has no title, this property
   *    isn't set.
   *  - dateAdded (number, microseconds from the epoch): the date-added value of
   *    the item.
   *  - lastModified (number, microseconds from the epoch): the last-modified
   *    value of the item.
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
   *  - iconUri (string): the bookmark's favicon url.
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
          if (val !== null) {
            item[prop] = val;
          }
        }
      };
      copyProps("guid", "title", "index", "dateAdded", "lastModified");
      if (aIncludeParentGuid) {
        copyProps("parentGuid");
      }

      let itemId = aRow.getResultByName("id");
      if (aOptions.includeItemIds) {
        item.id = itemId;
      }

      // Cache it for promiseItemId consumers regardless.
      GuidHelper.updateCache(itemId, item.guid);

      let type = aRow.getResultByName("type");
      item.typeCode = type;
      if (type == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
        copyProps("charset", "tags", "iconUri");
      }

      switch (type) {
        case PlacesUtils.bookmarks.TYPE_BOOKMARK:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE;
          // If this throws due to an invalid url, the item will be skipped.
          try {
            item.uri = lazy.NetUtil.newURI(aRow.getResultByName("url")).spec;
          } catch (ex) {
            let error = new Error("Invalid bookmark URL");
            error.becauseInvalidURL = true;
            throw error;
          }
          // Keywords are cached, so this should be decently fast.
          let entry = await PlacesUtils.keywords.fetch({ url: item.uri });
          if (entry) {
            item.keyword = entry.keyword;
            item.postData = entry.postData;
          }
          break;
        case PlacesUtils.bookmarks.TYPE_FOLDER:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
          // Mark root folders.
          if (item.guid == PlacesUtils.bookmarks.rootGuid) {
            item.root = "placesRoot";
          } else if (item.guid == PlacesUtils.bookmarks.menuGuid) {
            item.root = "bookmarksMenuFolder";
          } else if (item.guid == PlacesUtils.bookmarks.unfiledGuid) {
            item.root = "unfiledBookmarksFolder";
          } else if (item.guid == PlacesUtils.bookmarks.toolbarGuid) {
            item.root = "toolbarFolder";
          } else if (item.guid == PlacesUtils.bookmarks.mobileGuid) {
            item.root = "mobileFolder";
          }
          break;
        case PlacesUtils.bookmarks.TYPE_SEPARATOR:
          item.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
          break;
        default:
          Cu.reportError(`Unexpected bookmark type ${type}`);
          break;
      }
      return item;
    };

    const QUERY_STR = `/* do not warn (bug no): cannot use an index */
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
              d.position AS [index], IFNULL(d.title, '') AS title, d.dateAdded,
              d.lastModified, h.url, (SELECT icon_url FROM moz_icons i
                      JOIN moz_icons_to_pages ON icon_id = i.id
                      JOIN moz_pages_w_icons pi ON page_id = pi.id
                      WHERE pi.page_url_hash = hash(h.url) AND pi.page_url = h.url
                      ORDER BY width DESC LIMIT 1) AS iconUri,
              (SELECT GROUP_CONCAT(t.title, ',')
               FROM moz_bookmarks b2
               JOIN moz_bookmarks t ON t.id = +b2.parent AND t.parent = :tags_folder
               WHERE b2.fk = h.id
              ) AS tags,
              (SELECT a.content FROM moz_annos a
               JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id
               WHERE place_id = h.id AND n.name = :charset_anno
              ) AS charset
       FROM descendants d
       LEFT JOIN moz_bookmarks b3 ON b3.id = d.parent
       LEFT JOIN moz_places h ON h.id = d.fk
       ORDER BY d.level, d.parent, d.position`;

    if (!aItemGuid) {
      aItemGuid = this.bookmarks.rootGuid;
    }

    let hasExcludeItemsCallback = aOptions.hasOwnProperty(
      "excludeItemsCallback"
    );
    let excludedParents = new Set();
    let shouldExcludeItem = (aItem, aParentGuid) => {
      let exclude =
        excludedParents.has(aParentGuid) ||
        aOptions.excludeItemsCallback(aItem);
      if (exclude) {
        if (aItem.type == this.TYPE_X_MOZ_PLACE_CONTAINER) {
          excludedParents.add(aItem.guid);
        }
      }
      return exclude;
    };

    let rootItem = null;
    let parentsMap = new Map();
    let conn = await this.promiseDBConnection();
    let rows = await conn.executeCached(QUERY_STR, {
      tags_folder: PlacesUtils.tagsFolderId,
      charset_anno: PlacesUtils.CHARSET_ANNO,
      item_guid: aItemGuid,
    });
    let yieldCounter = 0;
    for (let row of rows) {
      let item;
      if (!rootItem) {
        try {
          // This is the first row.
          rootItem = item = await createItemInfoObject(row, true);
          Object.defineProperty(rootItem, "itemsCount", {
            value: 1,
            writable: true,
            enumerable: false,
            configurable: false,
          });
        } catch (ex) {
          Cu.reportError("Failed to fetch the data for the root item");
          throw ex;
        }
      } else {
        try {
          // Our query guarantees that we always visit parents ahead of their
          // children.
          item = await createItemInfoObject(row, false);
          let parentGuid = row.getResultByName("parentGuid");
          if (hasExcludeItemsCallback && shouldExcludeItem(item, parentGuid)) {
            continue;
          }

          let parentItem = parentsMap.get(parentGuid);
          if ("children" in parentItem) {
            parentItem.children.push(item);
          } else {
            parentItem.children = [item];
          }

          rootItem.itemsCount++;
        } catch (ex) {
          // This is a bogus child, report and skip it.
          Cu.reportError("Failed to fetch the data for an item " + ex);
          continue;
        }
      }

      if (item.type == this.TYPE_X_MOZ_PLACE_CONTAINER) {
        parentsMap.set(item.guid, item);
      }

      // With many bookmarks we end up stealing the CPU - even with yielding!
      // So we let everyone else have a go every few items (bug 1186714).
      if (++yieldCounter % 50 == 0) {
        await new Promise(resolve => {
          Services.tm.dispatchToMainThread(resolve);
        });
      }
    }

    return rootItem;
  },

  /**
   * Returns a generator that iterates over `array` and yields slices of no
   * more than `chunkLength` elements at a time.
   *
   * @param  {Array} array An array containing zero or more elements.
   * @param  {number} chunkLength The maximum number of elements in each chunk.
   * @yields {Array} A chunk of the array.
   * @throws if `chunkLength` is negative or not an integer.
   */
  *chunkArray(array, chunkLength) {
    if (chunkLength <= 0 || !Number.isInteger(chunkLength)) {
      throw new TypeError("Chunk length must be a positive integer");
    }
    if (!array.length) {
      return;
    }
    if (array.length <= chunkLength) {
      yield array;
      return;
    }
    let startIndex = 0;
    while (startIndex < array.length) {
      yield array.slice(startIndex, (startIndex += chunkLength));
    }
  },

  /**
   * Returns SQL placeholders to bind multiple values into an IN clause.
   * @param {Array|number} info
   *   Array or number of entries to create.
   * @param {string} [prefix]
   *   String prefix to add before the SQL param.
   * @param {string} [suffix]
   *   String suffix to add after the SQL param.
   */
  sqlBindPlaceholders(info, prefix = "", suffix = "") {
    let length = Array.isArray(info) ? info.length : info;
    return new Array(length).fill(prefix + "?" + suffix).join(",");
  },

  /**
   * Run some text through md5 and return the hash.
   * @param {string} data The string to hash.
   * @param {string} [format] Which format of the hash to return:
   *   - "ascii" for ascii format.
   *   - "hex" for hex format.
   * @returns {string} md5 hash of the input string in the required format.
   */
  md5(data, { format = "ascii" } = {}) {
    lazy.gCryptoHash.init(lazy.gCryptoHash.MD5);

    // Convert the data to a byte array for hashing
    lazy.gCryptoHash.update(
      data.split("").map(c => c.charCodeAt(0)),
      data.length
    );
    switch (format) {
      case "hex":
        let hash = lazy.gCryptoHash.finish(false);
        return Array.from(hash, (c, i) =>
          hash
            .charCodeAt(i)
            .toString(16)
            .padStart(2, "0")
        ).join("");
      case "ascii":
      default:
        return lazy.gCryptoHash.finish(true);
    }
  },

  /**
   * Inserts a new place if one doesn't currently exist.
   *
   * This should only be used from an API that is connecting this new entry to
   * some additional foreign table. Otherwise this will just create an orphan
   * entry that could be expired at any time.
   *
   * @param db
   *        The database connection to use.
   * @param url
   *        A valid URL object.
   * @return {Promise} resolved when the operation is complete.
   */
  async maybeInsertPlace(db, url) {
    // The IGNORE conflict can trigger on `guid`.
    await db.executeCached(
      `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid)
      VALUES (:url, hash(:url), :rev_host, 0, :frecency,
              IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
                      GENERATE_GUID()))
      `,
      {
        url: url.href,
        rev_host: this.getReversedHost(url),
        frecency: url.protocol == "place:" ? 0 : -1,
      }
    );
    await db.executeCached("DELETE FROM moz_updateoriginsinsert_temp");
  },

  /**
   * Tries to insert a set of new places if they don't exist yet.
   *
   * This should only be used from an API that is connecting this new entry to
   * some additional foreign table. Otherwise this will just create an orphan
   * entry that could be expired at any time.
   *
   * @param db
   *        The database to use
   * @param urls
   *        An array with all the url objects to insert.
   * @return {Promise} resolved when the operation is complete.
   */
  async maybeInsertManyPlaces(db, urls) {
    await db.executeCached(
      `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid) VALUES
     (:url, hash(:url), :rev_host, 0, :frecency,
     IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url), :maybeguid))`,
      urls.map(url => ({
        url: url.href,
        rev_host: this.getReversedHost(url),
        frecency: url.protocol == "place:" ? 0 : -1,
        maybeguid: this.history.makeGuid(),
      }))
    );
    await db.executeCached("DELETE FROM moz_updateoriginsinsert_temp");
  },
};

XPCOMUtils.defineLazyGetter(PlacesUtils, "history", function() {
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
    Ci.nsINavHistoryService
  );
  return Object.freeze(
    new Proxy(hs, {
      get(target, name) {
        let property, object;
        if (name in target) {
          property = target[name];
          object = target;
        } else {
          property = lazy.History[name];
          object = lazy.History;
        }
        if (typeof property == "function") {
          return property.bind(object);
        }
        return property;
      },
    })
  );
});

XPCOMUtils.defineLazyServiceGetter(
  PlacesUtils,
  "favicons",
  "@mozilla.org/browser/favicon-service;1",
  "nsIFaviconService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "bmsvc",
  "@mozilla.org/browser/nav-bookmarks-service;1",
  "nsINavBookmarksService"
);
XPCOMUtils.defineLazyGetter(PlacesUtils, "bookmarks", () => {
  return Object.freeze(
    new Proxy(lazy.Bookmarks, {
      get: (target, name) =>
        lazy.Bookmarks.hasOwnProperty(name)
          ? lazy.Bookmarks[name]
          : lazy.bmsvc[name],
    })
  );
});

XPCOMUtils.defineLazyServiceGetter(
  PlacesUtils,
  "tagging",
  "@mozilla.org/browser/tagging-service;1",
  "nsITaggingService"
);

XPCOMUtils.defineLazyGetter(lazy, "bundle", function() {
  const PLACES_STRING_BUNDLE_URI = "chrome://places/locale/places.properties";
  return Services.strings.createBundle(PLACES_STRING_BUNDLE_URI);
});

// This is just used as a reasonably-random value for copy & paste / drag operations.
XPCOMUtils.defineLazyGetter(PlacesUtils, "instanceId", () => {
  return PlacesUtils.history.makeGuid();
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
        PlacesUtils.history.connectionShutdownClient.jsclient.addBlocker(
          `${name} closing as part of Places shutdown`,
          async function() {
            state = "1. Service has initiated shutdown";

            // At this stage, all external clients have finished using the
            // database. We just need to close the high-level connection.
            try {
              await conn.close();
              state = "2. Closed Sqlite.jsm connection.";
              resolve();
            } catch (ex) {
              state = "2. Failed to closed Sqlite.jsm connection: " + ex;
              reject(ex);
            }
          },
          () => state
        );
      } catch (ex) {
        // It's too late to block shutdown, just close the connection.
        conn.close();
        reject(ex);
      }
    }).catch(Cu.reportError);

    // Make sure that Sqlite.jsm doesn't close until we are done
    // with the high-level connection.
    lazy.Sqlite.shutdown.addBlocker(
      `${name} must be closed before Sqlite.jsm`,
      () => promiseClosed,
      () => state
    );
  } catch (ex) {
    // It's too late to block shutdown, just close the connection.
    conn.close();
    throw ex;
  }
}

XPCOMUtils.defineLazyGetter(lazy, "gAsyncDBConnPromised", () =>
  lazy.Sqlite.cloneStorageConnection({
    connection: PlacesUtils.history.DBConnection,
    readOnly: true,
  })
    .then(conn => {
      setupDbForShutdown(conn, "PlacesUtils read-only connection");
      return conn;
    })
    .catch(Cu.reportError)
);

XPCOMUtils.defineLazyGetter(lazy, "gAsyncDBWrapperPromised", () =>
  lazy.Sqlite.wrapStorageConnection({
    connection: PlacesUtils.history.DBConnection,
  })
    .then(conn => {
      setupDbForShutdown(conn, "PlacesUtils wrapped connection");
      return conn;
    })
    .catch(Cu.reportError)
);

var gAsyncDBLargeCacheConnDeferred = PromiseUtils.defer();
XPCOMUtils.defineLazyGetter(lazy, "gAsyncDBLargeCacheConnPromised", () =>
  lazy.Sqlite.cloneStorageConnection({
    connection: PlacesUtils.history.DBConnection,
    readOnly: true,
  })
    .then(async conn => {
      setupDbForShutdown(conn, "PlacesUtils large cache read-only connection");
      // Components like the urlbar often fallback to a table scan due to lack
      // of full text indices.  A larger cache helps reducing IO and improves
      // performance. This value is expected to be larger than the default
      // mozStorage value defined as MAX_CACHE_SIZE_BYTES in
      // storage/mozStorageConnection.cpp.
      await conn.execute("PRAGMA cache_size = -6144"); // 6MiB
      // These should be kept in sync with nsPlacesTables.h.
      await conn.execute(`
        CREATE TEMP TABLE IF NOT EXISTS moz_openpages_temp (
          url TEXT,
          userContextId INTEGER,
          open_count INTEGER,
          PRIMARY KEY (url, userContextId)
        )`);
      await conn.execute(`
        CREATE TEMP TRIGGER IF NOT EXISTS moz_openpages_temp_afterupdate_trigger
        AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW
        WHEN NEW.open_count = 0
        BEGIN
          DELETE FROM moz_openpages_temp
          WHERE url = NEW.url
            AND userContextId = NEW.userContextId;
        END`);
      gAsyncDBLargeCacheConnDeferred.resolve(conn);
      return conn;
    })
    .catch(Cu.reportError)
);

/**
 * The metadata API allows consumers to store simple key-value metadata in
 * Places. Keys are strings, values can be any type that SQLite supports:
 * numbers (integers and doubles), Booleans, strings, and blobs. Values are
 * cached in memory for faster lookups.
 *
 * Since some consumers set metadata as part of an existing operation or active
 * transaction, the API also exposes a `*withConnection` variant for each
 * method that takes an open database connection.
 */
PlacesUtils.metadata = {
  cache: new Map(),
  jsonPrefix: "data:application/json;base64,",

  /**
   * Returns the value associated with a metadata key.
   *
   * @param  {String} key
   *         The metadata key to look up.
   * @param  {String|Object|Array} defaultValue
   *         Optional. The default value to return if the value is not present,
   *         or cannot be parsed.
   * @resolves {*}
   *         The value associated with the key, or the defaultValue if there is one.
   * @rejects
   *         Rejected if the value is not found or it cannot be parsed
   *         and there is no defaultValue.
   */
  get(key, defaultValue) {
    return PlacesUtils.withConnectionWrapper("PlacesUtils.metadata.get", db =>
      this.getWithConnection(db, key, defaultValue)
    );
  },

  /**
   * Sets the value for a metadata key.
   *
   * @param {String} key
   *        The metadata key to update.
   * @param {*}
   *        The value to associate with the key.
   */
  set(key, value) {
    return PlacesUtils.withConnectionWrapper("PlacesUtils.metadata.set", db =>
      this.setWithConnection(db, key, value)
    );
  },

  /**
   * Removes the values for the given metadata keys.
   *
   * @param {String...}
   *        One or more metadata keys to remove.
   */
  delete(...keys) {
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.metadata.delete",
      db => this.deleteWithConnection(db, ...keys)
    );
  },

  async getWithConnection(db, key, defaultValue) {
    key = this.canonicalizeKey(key);
    if (this.cache.has(key)) {
      return this.cache.get(key);
    }
    let rows = await db.executeCached(
      `
      SELECT value FROM moz_meta WHERE key = :key`,
      { key }
    );
    let value = null;
    if (rows.length) {
      let row = rows[0];
      let rawValue = row.getResultByName("value");
      // Convert blobs back to `Uint8Array`s.
      if (row.getTypeOfIndex(0) == row.VALUE_TYPE_BLOB) {
        value = new Uint8Array(rawValue);
      } else if (
        typeof rawValue == "string" &&
        rawValue.startsWith(this.jsonPrefix)
      ) {
        try {
          value = JSON.parse(
            this._base64Decode(rawValue.substr(this.jsonPrefix.length))
          );
        } catch (ex) {
          if (defaultValue !== undefined) {
            // We must create a new array in the local scope to avoid a memory
            // leak due to the array global object.
            value = Cu.cloneInto(defaultValue, {});
          } else {
            throw ex;
          }
        }
      } else {
        value = rawValue;
      }
    } else if (defaultValue !== undefined) {
      // We must create a new array in the local scope to avoid a memory leak due
      // to the array global object.
      value = Cu.cloneInto(defaultValue, {});
    } else {
      throw new Error(`No data stored for key ${key}`);
    }
    this.cache.set(key, value);
    return value;
  },

  async setWithConnection(db, key, value) {
    if (value === null) {
      await this.deleteWithConnection(db, key);
      return;
    }

    let cacheValue = value;
    if (
      typeof value == "object" &&
      ChromeUtils.getClassName(value) != "Uint8Array"
    ) {
      value = this.jsonPrefix + this._base64Encode(JSON.stringify(value));
    }

    key = this.canonicalizeKey(key);
    await db.executeCached(
      `
      REPLACE INTO moz_meta (key, value)
      VALUES (:key, :value)`,
      { key, value }
    );
    this.cache.set(key, cacheValue);
  },

  async deleteWithConnection(db, ...keys) {
    keys = keys.map(this.canonicalizeKey);
    if (!keys.length) {
      return;
    }
    await db.execute(
      `
      DELETE FROM moz_meta
      WHERE key IN (${new Array(keys.length).fill("?").join(",")})`,
      keys
    );
    for (let key of keys) {
      this.cache.delete(key);
    }
  },

  canonicalizeKey(key) {
    if (typeof key != "string" || !/^[a-zA-Z0-9\/]+$/.test(key)) {
      throw new TypeError("Invalid metadata key: " + key);
    }
    return key.toLowerCase();
  },

  _base64Encode(str) {
    return ChromeUtils.base64URLEncode(new TextEncoder("utf-8").encode(str), {
      pad: true,
    });
  },

  _base64Decode(str) {
    return new TextDecoder("utf-8").decode(
      ChromeUtils.base64URLDecode(str, { padding: "require" })
    );
  },
};

/**
 * Keywords management API.
 * Sooner or later these keywords will merge with search aliases, this is an
 * interim API that should then be replaced by a unified one.
 * Keywords are associated with URLs and can have POST data.
 * The relations between URLs and keywords are the following:
 *  - 1 keyword can only point to 1 URL
 *  - 1 URL can have multiple keywords, iff they differ by POST data (included the empty one).
 */
PlacesUtils.keywords = {
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
    if (typeof keywordOrEntry == "string") {
      keywordOrEntry = { keyword: keywordOrEntry };
    }

    if (
      keywordOrEntry === null ||
      typeof keywordOrEntry != "object" ||
      ("keyword" in keywordOrEntry && typeof keywordOrEntry.keyword != "string")
    ) {
      throw new Error("Invalid keyword");
    }

    let hasKeyword = "keyword" in keywordOrEntry;
    let hasUrl = "url" in keywordOrEntry;

    if (!hasKeyword && !hasUrl) {
      throw new Error("At least keyword or url must be provided");
    }
    if (onResult && typeof onResult != "function") {
      throw new Error("onResult callback must be a valid function");
    }

    if (hasUrl) {
      try {
        keywordOrEntry.url = BOOKMARK_VALIDATORS.url(keywordOrEntry.url);
      } catch (ex) {
        throw new Error(keywordOrEntry.url + " is not a valid URL");
      }
    }
    if (hasKeyword) {
      keywordOrEntry.keyword = keywordOrEntry.keyword.trim().toLowerCase();
    }

    let safeOnResult = entry => {
      if (onResult) {
        try {
          onResult(entry);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    };

    return promiseKeywordsCache().then(cache => {
      let entries = [];
      if (hasKeyword) {
        let entry = cache.get(keywordOrEntry.keyword);
        if (entry) {
          entries.push(entry);
        }
      }
      if (hasUrl) {
        for (let entry of cache.values()) {
          if (entry.url.href == keywordOrEntry.url.href) {
            entries.push(entry);
          }
        }
      }

      entries = entries.filter(e => {
        return (
          (!hasUrl || e.url.href == keywordOrEntry.url.href) &&
          (!hasKeyword || e.keyword == keywordOrEntry.keyword)
        );
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
   *          url: URL or href to associate to the keyword,
   *          postData: optional POST data to associate to the keyword
   *          source: The change source, forwarded to all bookmark observers.
   *            Defaults to nsINavBookmarksService::SOURCE_DEFAULT.
   *        }
   * @note Do not define a postData property if there isn't any POST data.
   *       Defining an empty string for POST data is equivalent to not having it.
   * @resolves when the addition is complete.
   */
  insert(keywordEntry) {
    if (!keywordEntry || typeof keywordEntry != "object") {
      throw new Error("Input should be a valid object");
    }

    if (
      !("keyword" in keywordEntry) ||
      !keywordEntry.keyword ||
      typeof keywordEntry.keyword != "string"
    ) {
      throw new Error("Invalid keyword");
    }
    if (
      "postData" in keywordEntry &&
      keywordEntry.postData &&
      typeof keywordEntry.postData != "string"
    ) {
      throw new Error("Invalid POST data");
    }
    if (!("url" in keywordEntry)) {
      throw new Error("undefined is not a valid URL");
    }

    if (!("source" in keywordEntry)) {
      keywordEntry.source = PlacesUtils.bookmarks.SOURCES.DEFAULT;
    }
    let { keyword, url, source } = keywordEntry;
    keyword = keyword.trim().toLowerCase();
    let postData = keywordEntry.postData || "";
    // This also checks href for validity
    try {
      url = BOOKMARK_VALIDATORS.url(url);
    } catch (ex) {
      throw new Error(url + " is not a valid URL");
    }

    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.keywords.insert",
      async db => {
        let cache = await promiseKeywordsCache();

        // Trying to set the same keyword is a no-op.
        let oldEntry = cache.get(keyword);
        if (
          oldEntry &&
          oldEntry.url.href == url.href &&
          (oldEntry.postData || "") == postData
        ) {
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
            `,
            { url: url.href, keyword, post_data: postData }
          );
          await notifyKeywordChange(oldEntry.url.href, "", source);
        } else {
          // An entry for the given page could be missing, in such a case we need to
          // create it.  The IGNORE conflict can trigger on `guid`.
          await db.executeTransaction(async () => {
            await PlacesUtils.maybeInsertPlace(db, url);

            // A new keyword could be assigned to an url that already has one,
            // then we must replace the old keyword with the new one.
            let oldKeywords = [];
            for (let entry of cache.values()) {
              if (
                entry.url.href == url.href &&
                (entry.postData || "") == postData
              ) {
                oldKeywords.push(entry.keyword);
              }
            }
            if (oldKeywords.length) {
              for (let oldKeyword of oldKeywords) {
                await db.executeCached(
                  `DELETE FROM moz_keywords WHERE keyword = :oldKeyword`,
                  { oldKeyword }
                );
                cache.delete(oldKeyword);
              }
            }

            await db.executeCached(
              `INSERT INTO moz_keywords (keyword, place_id, post_data)
               VALUES (:keyword, (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url), :post_data)
              `,
              { url: url.href, keyword, post_data: postData }
            );

            await lazy.PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
              db,
              url,
              lazy.PlacesSyncUtils.bookmarks.determineSyncChangeDelta(source)
            );
          });
        }

        cache.set(keyword, { keyword, url, postData: postData || null });

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
    if (typeof keywordOrEntry == "string") {
      keywordOrEntry = {
        keyword: keywordOrEntry,
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      };
    }

    if (
      keywordOrEntry === null ||
      typeof keywordOrEntry != "object" ||
      !keywordOrEntry.keyword ||
      typeof keywordOrEntry.keyword != "string"
    ) {
      throw new Error("Invalid keyword");
    }

    let {
      keyword,
      source = Ci.nsINavBookmarksService.SOURCE_DEFAULT,
    } = keywordOrEntry;
    keyword = keywordOrEntry.keyword.trim().toLowerCase();
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.keywords.remove",
      async db => {
        let cache = await promiseKeywordsCache();
        if (!cache.has(keyword)) {
          return;
        }
        let { url } = cache.get(keyword);
        cache.delete(keyword);

        await db.executeTransaction(async function() {
          await db.execute(
            `DELETE FROM moz_keywords WHERE keyword = :keyword`,
            { keyword }
          );

          await lazy.PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
            db,
            url,
            lazy.PlacesSyncUtils.bookmarks.determineSyncChangeDelta(source)
          );
        });

        // Notify bookmarks about the removal.
        await notifyKeywordChange(url.href, "", source);
      }
    );
  },

  /**
   * Moves all (keyword, POST data) pairs from one URL to another, and fires
   * observer notifications for all affected bookmarks. If the destination URL
   * already has keywords, they will be removed and replaced with the source
   * URL's keywords.
   *
   * @param oldURL
   *        The source URL.
   * @param newURL
   *        The destination URL.
   * @param source
   *        The change source, forwarded to all bookmark observers.
   * @return {Promise}
   * @resolves when all keywords have been moved to the destination URL.
   */
  reassign(oldURL, newURL, source = PlacesUtils.bookmarks.SOURCES.DEFAULT) {
    try {
      oldURL = BOOKMARK_VALIDATORS.url(oldURL);
    } catch (ex) {
      throw new Error(oldURL + " is not a valid source URL");
    }
    try {
      newURL = BOOKMARK_VALIDATORS.url(newURL);
    } catch (ex) {
      throw new Error(oldURL + " is not a valid destination URL");
    }
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.keywords.reassign",
      async function(db) {
        let keywordsToReassign = [];
        let keywordsToRemove = [];
        let cache = await promiseKeywordsCache();
        for (let [keyword, entry] of cache) {
          if (entry.url.href == oldURL.href) {
            keywordsToReassign.push(keyword);
          }
          if (entry.url.href == newURL.href) {
            keywordsToRemove.push(keyword);
          }
        }
        if (!keywordsToReassign.length) {
          return;
        }

        await db.executeTransaction(async function() {
          // Remove existing keywords from the new URL.
          await db.executeCached(
            `DELETE FROM moz_keywords WHERE keyword = :keyword`,
            keywordsToRemove.map(keyword => ({ keyword }))
          );

          // Move keywords from the old URL to the new URL.
          await db.executeCached(
            `
          UPDATE moz_keywords SET
            place_id = (SELECT id FROM moz_places
                        WHERE url_hash = hash(:newURL) AND
                              url = :newURL)
          WHERE place_id = (SELECT id FROM moz_places
                            WHERE url_hash = hash(:oldURL) AND
                                  url = :oldURL)`,
            { newURL: newURL.href, oldURL: oldURL.href }
          );
        });
        for (let keyword of keywordsToReassign) {
          let entry = cache.get(keyword);
          entry.url = newURL;
        }
        for (let keyword of keywordsToRemove) {
          cache.delete(keyword);
        }

        if (keywordsToReassign.length) {
          // If we moved any keywords, notify that we removed all keywords from
          // the old and new URLs, then notify for each moved keyword.
          await notifyKeywordChange(oldURL, "", source);
          await notifyKeywordChange(newURL, "", source);
          for (let keyword of keywordsToReassign) {
            await notifyKeywordChange(newURL, keyword, source);
          }
        } else if (keywordsToRemove.length) {
          // If the old URL didn't have any keywords, but the new URL did, just
          // notify that we removed all keywords from the new URL.
          await notifyKeywordChange(oldURL, "", source);
        }
      }
    );
  },

  /**
   * Removes all orphaned keywords from the given URLs. Orphaned keywords are
   * associated with URLs that are no longer bookmarked. If a given URL is still
   * bookmarked, its keywords will not be removed.
   *
   * @param urls
   *        A list of URLs to check for orphaned keywords.
   * @return {Promise}
   * @resolves when all keywords have been removed from URLs that are no longer
   *           bookmarked.
   */
  removeFromURLsIfNotBookmarked(urls) {
    let hrefs = new Set();
    for (let url of urls) {
      try {
        url = BOOKMARK_VALIDATORS.url(url);
      } catch (ex) {
        throw new Error(url + " is not a valid URL");
      }
      hrefs.add(url.href);
    }
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.keywords.removeFromURLsIfNotBookmarked",
      async function(db) {
        let keywordsByHref = new Map();
        let cache = await promiseKeywordsCache();
        for (let [keyword, entry] of cache) {
          let href = entry.url.href;
          if (!hrefs.has(href)) {
            continue;
          }
          if (!keywordsByHref.has(href)) {
            keywordsByHref.set(href, [keyword]);
            continue;
          }
          let existingKeywords = keywordsByHref.get(href);
          existingKeywords.push(keyword);
        }
        if (!keywordsByHref.size) {
          return;
        }

        let placeInfosToRemove = [];
        let rows = await db.execute(
          `
          SELECT h.id, h.url
          FROM moz_places h
          JOIN moz_keywords k ON k.place_id = h.id
          GROUP BY h.id
          HAVING h.foreign_count = count(*) +
            (SELECT count(*)
             FROM moz_bookmarks b
             JOIN moz_bookmarks p ON b.parent = p.id
             WHERE p.parent = :tags_root AND b.fk = h.id)
          `,
          { tags_root: PlacesUtils.tagsFolderId }
        );
        for (let row of rows) {
          placeInfosToRemove.push({
            placeId: row.getResultByName("id"),
            href: row.getResultByName("url"),
          });
        }
        if (!placeInfosToRemove.length) {
          return;
        }

        await db.execute(
          `DELETE FROM moz_keywords WHERE place_id IN (${Array.from(
            placeInfosToRemove.map(info => info.placeId)
          ).join()})`
        );
        for (let { href } of placeInfosToRemove) {
          let keywords = keywordsByHref.get(href);
          for (let keyword of keywords) {
            cache.delete(keyword);
          }
        }
      }
    );
  },

  /**
   * Removes all keywords from all URLs.
   *
   * @return {Promise}
   * @resolves when all keywords have been removed.
   */
  eraseEverything() {
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils.keywords.eraseEverything",
      async function(db) {
        let cache = await promiseKeywordsCache();
        if (!cache.size) {
          return;
        }
        await db.executeCached(`DELETE FROM moz_keywords`);
        cache.clear();
      }
    );
  },

  /**
   * Invalidates the keywords cache, leaving all existing keywords in place.
   * The cache will be repopulated on the next `PlacesUtils.keywords.*` call.
   *
   * @return {Promise}
   * @resolves when the cache has been cleared.
   */
  invalidateCachedKeywords() {
    gKeywordsCachePromise = gKeywordsCachePromise.then(_ => null);
    return gKeywordsCachePromise;
  },
};

var gKeywordsCachePromise = Promise.resolve();

function promiseKeywordsCache() {
  let promise = gKeywordsCachePromise.then(function(cache) {
    if (cache) {
      return cache;
    }
    return PlacesUtils.withConnectionWrapper(
      "PlacesUtils: promiseKeywordsCache",
      async db => {
        let cache = new Map();
        let rows = await db.execute(
          `SELECT keyword, url, post_data
           FROM moz_keywords k
           JOIN moz_places h ON h.id = k.place_id
          `
        );
        let brokenKeywords = [];
        for (let row of rows) {
          let keyword = row.getResultByName("keyword");
          try {
            let entry = {
              keyword,
              url: new URL(row.getResultByName("url")),
              postData: row.getResultByName("post_data") || null,
            };
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
            `
          );
        }
        return cache;
      }
    );
  });
  gKeywordsCachePromise = promise.catch(_ => {});
  return promise;
}

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
    if (cached !== undefined) {
      return cached;
    }

    let itemId = await PlacesUtils.withConnectionWrapper(
      "GuidHelper.getItemId",
      async function(db) {
        let rows = await db.executeCached(
          "SELECT b.id, b.guid from moz_bookmarks b WHERE b.guid = :guid LIMIT 1",
          { guid: aGuid }
        );
        if (!rows.length) {
          throw new Error("no item found for the given GUID");
        }

        return rows[0].getResultByName("id");
      }
    );

    this.updateCache(itemId, aGuid);
    return itemId;
  },

  async getManyItemIds(aGuids) {
    let uncachedGuids = aGuids.filter(guid => !this.idsForGuids.has(guid));
    if (uncachedGuids.length) {
      await PlacesUtils.withConnectionWrapper(
        "GuidHelper.getItemId",
        async db => {
          while (uncachedGuids.length) {
            let chunk = uncachedGuids.splice(0, 100);
            let rows = await db.executeCached(
              `SELECT b.id, b.guid from moz_bookmarks b WHERE
             b.guid IN (${"?,".repeat(chunk.length - 1) + "?"})
             LIMIT ${chunk.length}`,
              chunk
            );
            if (rows.length < chunk.length) {
              throw new Error("Not all items were found!");
            }
            for (let row of rows) {
              this.updateCache(
                row.getResultByIndex(0),
                row.getResultByIndex(1)
              );
            }
          }
        }
      );
    }
    return new Map(aGuids.map(guid => [guid, this.idsForGuids.get(guid)]));
  },

  async getItemGuid(aItemId) {
    let cached = this.guidsForIds.get(aItemId);
    if (cached !== undefined) {
      return cached;
    }

    let guid = await PlacesUtils.withConnectionWrapper(
      "GuidHelper.getItemGuid",
      async function(db) {
        let rows = await db.executeCached(
          "SELECT b.id, b.guid from moz_bookmarks b WHERE b.id = :id LIMIT 1",
          { id: aItemId }
        );
        if (!rows.length) {
          throw new Error("no item found for the given itemId");
        }

        return rows[0].getResultByName("guid");
      }
    );

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
    if (typeof aItemId != "number" || aItemId <= 0) {
      throw new Error(
        "Trying to update the GUIDs cache with an invalid itemId"
      );
    }
    if (!PlacesUtils.isValidGuid(aGuid)) {
      throw new Error("Trying to update the GUIDs cache with an invalid GUID");
    }
    this.ensureObservingRemovedItems();
    this.guidsForIds.set(aItemId, aGuid);
    this.idsForGuids.set(aGuid, aItemId);
  },

  invalidateCacheForItemId(aItemId) {
    let guid = this.guidsForIds.get(aItemId);
    this.guidsForIds.delete(aItemId);
    this.idsForGuids.delete(guid);
  },

  invalidateCache() {
    this.guidsForIds.clear();
    this.idsForGuids.clear();
  },

  ensureObservingRemovedItems() {
    if (this.addListeners) {
      return;
    }
    /**
     * This observers serves two purposes:
     * (1) Invalidate cached id<->GUID paris on when items are removed.
     * (2) Cache GUIDs given us free of charge by onItemAdded/onItemRemoved.
     *      So, for exmaple, when the NewBookmark needs the new GUID, we already
     *      have it cached.
     */
    let listener = events => {
      for (let event of events) {
        switch (event.type) {
          case "bookmark-added":
            this.updateCache(event.id, event.guid);
            this.updateCache(event.parentId, event.parentGuid);
            break;
          case "bookmark-removed":
            this.guidsForIds.delete(event.id);
            this.idsForGuids.delete(event.guid);
            this.updateCache(event.parentId, event.parentGuid);
            break;
        }
      }
    };

    this.addListeners = true;
    PlacesUtils.observers.addListener(
      ["bookmark-added", "bookmark-removed"],
      listener
    );
    PlacesUtils.registerShutdownFunction(() => {
      PlacesUtils.observers.removeListener(
        ["bookmark-added", "bookmark-removed"],
        listener
      );
    });
  },
};
