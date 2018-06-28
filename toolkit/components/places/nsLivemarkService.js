/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Modules and services.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "NetUtil",
                               "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "history", function() {
  let livemarks = PlacesUtils.livemarks;
  // Lazily add an history observer when it's actually needed.
  PlacesUtils.history.addObserver(livemarks, true);
  let listener = new PlacesWeakCallbackWrapper(
    livemarks.handlePlacesEvents.bind(livemarks));
  PlacesObservers.addListener(["page-visited"], listener);
  return PlacesUtils.history;
});

// Constants

// Delay between reloads of consecute livemarks.
const RELOAD_DELAY_MS = 500;
// Expire livemarks after this time.
const EXPIRE_TIME_MS = 3600000; // 1 hour.
// Expire livemarks after this time on error.
const ONERROR_EXPIRE_TIME_MS = 300000; // 5 minutes.

// Livemarks cache.

XPCOMUtils.defineLazyGetter(this, "CACHE_SQL", () => {
  function getAnnoSQLFragment(aAnnoParam) {
    return `SELECT a.content
            FROM moz_items_annos a
            JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
            WHERE a.item_id = b.id
              AND n.name = ${aAnnoParam}`;
  }

  return `SELECT b.id, b.title, b.parent As parentId, b.position AS 'index',
                 b.guid, b.dateAdded, b.lastModified, p.guid AS parentGuid,
                 ( ${getAnnoSQLFragment(":feedURI_anno")} ) AS feedURI,
                 ( ${getAnnoSQLFragment(":siteURI_anno")} ) AS siteURI
          FROM moz_bookmarks b
          JOIN moz_bookmarks p ON b.parent = p.id
          JOIN moz_items_annos a ON a.item_id = b.id
          JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id
          WHERE b.type = :folder_type
            AND n.name = :feedURI_anno`;
});

/**
 * Convert a Date object to a PRTime (microseconds).
 *
 * @param date
 *        the Date object to convert.
 * @return microseconds from the epoch.
 */
function toPRTime(date) {
  return date * 1000;
}

/**
 * Convert a PRTime to a Date object.
 *
 * @param time
 *        microseconds from the epoch.
 * @return a Date object or undefined if time was not defined.
 */
function toDate(time) {
  return time ? new Date(parseInt(time / 1000)) : undefined;
}

// LivemarkService

function LivemarkService() {
  // Cleanup on shutdown.
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN, true);

  // Observe bookmarks but don't init the service just for that.
  PlacesUtils.bookmarks.addObserver(this, true);

  this._livemarksMap = null;
  this._promiseLivemarksMapReady = Promise.resolve();
}

LivemarkService.prototype = {
  _withLivemarksMap(func) {
    let promise = this._promiseLivemarksMapReady.then(async () => {
      if (!this._livemarksMap) {
        this._livemarksMap = new Map();
        let conn = await PlacesUtils.promiseDBConnection();
        let rows = await conn.executeCached(CACHE_SQL,
          { folder_type: Ci.nsINavBookmarksService.TYPE_FOLDER,
            feedURI_anno: PlacesUtils.LMANNO_FEEDURI,
            siteURI_anno: PlacesUtils.LMANNO_SITEURI });
        for (let row of rows) {
          let siteURI = row.getResultByName("siteURI");
          let livemark = new Livemark({
            id: row.getResultByName("id"),
            guid: row.getResultByName("guid"),
            title: row.getResultByName("title"),
            parentId: row.getResultByName("parentId"),
            parentGuid: row.getResultByName("parentGuid"),
            index: row.getResultByName("index"),
            dateAdded: row.getResultByName("dateAdded"),
            lastModified: row.getResultByName("lastModified"),
            feedURI: NetUtil.newURI(row.getResultByName("feedURI")),
            siteURI: siteURI ? NetUtil.newURI(siteURI) : null
          });
          this._livemarksMap.set(livemark.guid, livemark);
        }
      }
      return func(this._livemarksMap);
    });
    this._promiseLivemarksMapReady = promise.catch(_ => {});
    return promise;
  },

  _reloading: false,
  _startReloadTimer(livemarksMap, forceUpdate, reloaded) {
    if (this._reloadTimer) {
      this._reloadTimer.cancel();
    } else {
      this._reloadTimer = Cc["@mozilla.org/timer;1"]
                            .createInstance(Ci.nsITimer);
    }

    this._reloading = true;
    this._reloadTimer.initWithCallback(() => {
      // Find first livemark to be reloaded.
      for (let [ guid, livemark ] of livemarksMap) {
        if (!reloaded.has(guid)) {
          reloaded.add(guid);
          livemark.reload(forceUpdate);
          this._startReloadTimer(livemarksMap, forceUpdate, reloaded);
          return;
        }
      }
      // All livemarks have been reloaded.
      this._reloading = false;
      this._forceUpdate = false;
    }, RELOAD_DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  // nsIObserver

  observe(aSubject, aTopic, aData) {
    if (aTopic == PlacesUtils.TOPIC_SHUTDOWN) {
      this._invalidateCachedLivemarks({
        // No need to restart the reload timer on shutdown.
        keepReloading: false,
      }).catch(Cu.reportError);
    }
  },

  // mozIAsyncLivemarks

  addLivemark(aLivemarkInfo) {
    if (!aLivemarkInfo) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }
    let hasParentId = "parentId" in aLivemarkInfo;
    let hasParentGuid = "parentGuid" in aLivemarkInfo;
    let hasIndex = "index" in aLivemarkInfo;
    // Must provide at least non-null parent guid/id, index and feedURI.
    if ((!hasParentId && !hasParentGuid) ||
        (hasParentId && aLivemarkInfo.parentId < 1) ||
        (hasParentGuid && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.parentGuid)) ||
        (hasIndex && aLivemarkInfo.index < Ci.nsINavBookmarksService.DEFAULT_INDEX) ||
        !(aLivemarkInfo.feedURI instanceof Ci.nsIURI) ||
        (aLivemarkInfo.siteURI && !(aLivemarkInfo.siteURI instanceof Ci.nsIURI)) ||
        (aLivemarkInfo.guid && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid))) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }

    return this._withLivemarksMap(async livemarksMap => {
      if (!aLivemarkInfo.parentGuid)
        aLivemarkInfo.parentGuid = await PlacesUtils.promiseItemGuid(aLivemarkInfo.parentId);

      // Disallow adding a livemark inside another livemark.
      if (livemarksMap.has(aLivemarkInfo.parentGuid)) {
        throw new Components.Exception("Cannot create a livemark inside a livemark", Cr.NS_ERROR_INVALID_ARG);
      }

      // Create a new livemark.
      let folder = await PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid: aLivemarkInfo.parentGuid,
        title: aLivemarkInfo.title,
        index: aLivemarkInfo.index,
        guid: aLivemarkInfo.guid,
        dateAdded: toDate(aLivemarkInfo.dateAdded) || toDate(aLivemarkInfo.lastModified),
        source: aLivemarkInfo.source,
      });

      // Set feed and site URI annotations.
      let id = await PlacesUtils.promiseItemId(folder.guid);

      // Create the internal Livemark object.
      let livemark = new Livemark({ id,
                                    title:        folder.title,
                                    parentGuid:   folder.parentGuid,
                                    parentId:     await PlacesUtils.promiseItemId(folder.parentGuid),
                                    index:        folder.index,
                                    feedURI:      aLivemarkInfo.feedURI,
                                    siteURI:      aLivemarkInfo.siteURI,
                                    guid:         folder.guid,
                                    dateAdded:    toPRTime(folder.dateAdded),
                                    lastModified: toPRTime(folder.lastModified)
                                  });

      livemark.writeFeedURI(aLivemarkInfo.feedURI, aLivemarkInfo.source);
      if (aLivemarkInfo.siteURI) {
        livemark.writeSiteURI(aLivemarkInfo.siteURI, aLivemarkInfo.source);
      }

      if (aLivemarkInfo.lastModified) {
        await PlacesUtils.bookmarks.update({ guid: folder.guid,
                                             lastModified: toDate(aLivemarkInfo.lastModified),
                                             source: aLivemarkInfo.source });
        livemark.lastModified = aLivemarkInfo.lastModified;
      }

      livemarksMap.set(folder.guid, livemark);

      return livemark;
    });
  },

  removeLivemark(aLivemarkInfo) {
    if (!aLivemarkInfo) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }
    // Accept either a guid or an id.
    let hasGuid = "guid" in aLivemarkInfo;
    let hasId = "id" in aLivemarkInfo;
    if ((hasGuid && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid)) ||
        (hasId && aLivemarkInfo.id < 1) ||
        (!hasId && !hasGuid)) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }

    return this._withLivemarksMap(async livemarksMap => {
      if (!aLivemarkInfo.guid)
        aLivemarkInfo.guid = await PlacesUtils.promiseItemGuid(aLivemarkInfo.id);

      if (!livemarksMap.has(aLivemarkInfo.guid))
        throw new Components.Exception("Invalid livemark", Cr.NS_ERROR_INVALID_ARG);

      await PlacesUtils.bookmarks.remove(aLivemarkInfo.guid,
                                         { source: aLivemarkInfo.source });
    });
  },

  reloadLivemarks(aForceUpdate) {
    // Check if there's a currently running reload, to save some useless work.
    let notWorthRestarting =
      this._forceUpdate || // We're already forceUpdating.
      !aForceUpdate; // The caller didn't request a forced update.
    if (this._reloading && notWorthRestarting) {
      // Ignore this call.
      return;
    }

    this._withLivemarksMap(livemarksMap => {
      this._forceUpdate = !!aForceUpdate;
      // Livemarks reloads happen on a timer for performance reasons.
      this._startReloadTimer(livemarksMap, this._forceUpdate, new Set());
    });
  },

  getLivemark(aLivemarkInfo) {
    if (!aLivemarkInfo) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }
    // Accept either a guid or an id.
    let hasGuid = "guid" in aLivemarkInfo;
    let hasId = "id" in aLivemarkInfo;
    if ((hasGuid && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid)) ||
        (hasId && aLivemarkInfo.id < 1) ||
        (!hasId && !hasGuid)) {
      throw new Components.Exception("Invalid arguments", Cr.NS_ERROR_INVALID_ARG);
    }

    return this._withLivemarksMap(async livemarksMap => {
      if (!aLivemarkInfo.guid)
        aLivemarkInfo.guid = await PlacesUtils.promiseItemGuid(aLivemarkInfo.id);

      if (!livemarksMap.has(aLivemarkInfo.guid))
        throw new Components.Exception("Invalid livemark", Cr.NS_ERROR_INVALID_ARG);

      return livemarksMap.get(aLivemarkInfo.guid);
    });
  },

  _invalidateCachedLivemarks({ keepReloading = true } = {}) {
    // Cancel pending reloads, since any livemarks we're currently reloading
    // might no longer be valid.
    let wasReloading = this._reloading;
    this._reloading = false;

    let wasForceUpdating = this._forceUpdate;
    this._forceUpdate = false;

    if (this._reloadTimer) {
      this._reloadTimer.cancel();
    }

    // Clear out the livemarks cache.
    let promise = this._promiseLivemarksMapReady.then(() => {
      let livemarksMap = this._livemarksMap;
      this._livemarksMap = null;
      if (livemarksMap) {
        // Stop any ongoing network fetch.
        for (let livemark of livemarksMap.values()) {
          livemark.terminate();
        }
      }
    });
    this._promiseLivemarksMapReady = promise.catch(_ => {});

    // Restart the timer if we were reloading before invalidating.
    if (keepReloading) {
      if (wasReloading) {
        this.reloadLivemarks(wasForceUpdating);
      }
    } else {
      delete this._reloadTimer;
    }

    return promise;
  },

  invalidateCachedLivemarks() {
    return this._invalidateCachedLivemarks();
  },

  handlePlacesEvents(aEvents) {
    if (!aEvents) {
      throw new Components.Exception("Invalid arguments",
                                     Cr.NS_ERROR_INVALID_ARG);
    }

    this._withLivemarksMap(livemarksMap => {
      for (let event of aEvents) {
        for (let livemark of livemarksMap.values()) {
          livemark.updateURIVisitedStatus(event.url, true);
        }
      }
    });
  },

  // nsINavBookmarkObserver

  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onItemVisited() {},
  onItemAdded() {},

  onItemChanged(id, property, isAnno, value, lastModified, itemType, parentId,
                guid, parentGuid) {
    if (itemType != Ci.nsINavBookmarksService.TYPE_FOLDER)
      return;

    this._withLivemarksMap(livemarksMap => {
      if (livemarksMap.has(guid)) {
        let livemark = livemarksMap.get(guid);
        if (property == "title") {
          livemark.title = value;
        }
        livemark.lastModified = lastModified;
      }
    });
  },

  onItemMoved(id, parentId, oldIndex, newParentId, newIndex, itemType, guid,
              oldParentGuid, newParentGuid) {
    if (itemType != Ci.nsINavBookmarksService.TYPE_FOLDER)
      return;

    this._withLivemarksMap(livemarksMap => {
      if (livemarksMap.has(guid)) {
        let livemark = livemarksMap.get(guid);
        livemark.parentId = newParentId;
        livemark.parentGuid = newParentGuid;
        livemark.index = newIndex;
      }
    });
  },

  onItemRemoved(id, parentId, index, itemType, uri, guid, parentGuid) {
    if (itemType != Ci.nsINavBookmarksService.TYPE_FOLDER)
      return;

    this._withLivemarksMap(livemarksMap => {
      if (livemarksMap.has(guid)) {
        let livemark = livemarksMap.get(guid);
        livemark.terminate();
        livemarksMap.delete(guid);
      }
    });
  },

  skipDescendantsOnItemRemoval: false,
  skipTags: true,

  // nsINavHistoryObserver

  onPageChanged() {},
  onTitleChanged() {},
  onDeleteVisits() {},

  onClearHistory() {
    this._withLivemarksMap(livemarksMap => {
      for (let livemark of livemarksMap.values()) {
        livemark.updateURIVisitedStatus(null, false);
      }
    });
  },

  onDeleteURI(aURI) {
    this._withLivemarksMap(livemarksMap => {
      for (let livemark of livemarksMap.values()) {
        livemark.updateURIVisitedStatus(aURI, false);
      }
    });
  },

  // nsISupports

  classID: Components.ID("{dca61eb5-c7cd-4df1-b0fb-d0722baba251}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(LivemarkService),

  QueryInterface: ChromeUtils.generateQI([
    Ci.mozIAsyncLivemarks,
    Ci.nsINavBookmarkObserver,
    Ci.nsINavHistoryObserver,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};

// Livemark

/**
 * Object used internally to represent a livemark.
 *
 * @param aLivemarkInfo
 *        Object containing information on the livemark.  If the livemark is
 *        not included in the object, a new livemark will be created.
 *
 * @note terminate() must be invoked before getting rid of this object.
 */
function Livemark(aLivemarkInfo) {
  this.id = aLivemarkInfo.id;
  this.guid = aLivemarkInfo.guid;
  this.feedURI = aLivemarkInfo.feedURI;
  this.siteURI = aLivemarkInfo.siteURI || null;
  this.title = aLivemarkInfo.title;
  this.parentId = aLivemarkInfo.parentId;
  this.parentGuid = aLivemarkInfo.parentGuid;
  this.index = aLivemarkInfo.index;
  this.dateAdded = aLivemarkInfo.dateAdded;
  this.lastModified = aLivemarkInfo.lastModified;

  this._status = Ci.mozILivemark.STATUS_READY;

  // Hash of resultObservers, hashed by container.
  this._resultObservers = new Map();

  // Sorted array of objects representing livemark children in the form
  // { uri, title, visited }.
  this._children = [];

  // Keeps a separate array of nodes for each requesting container, hashed by
  // the container itself.
  this._nodes = new Map();

  this.loadGroup = null;
  this.expireTime = 0;
}

Livemark.prototype = {
  get status() {
    return this._status;
  },
  set status(val) {
    if (this._status != val) {
      this._status = val;
      this._invalidateRegisteredContainers();
    }
    return this._status;
  },

  writeFeedURI(aFeedURI, aSource) {
    PlacesUtils.annotations
               .setItemAnnotation(this.id, PlacesUtils.LMANNO_FEEDURI,
                                  aFeedURI.spec,
                                  0, PlacesUtils.annotations.EXPIRE_NEVER,
                                  aSource, true);
    this.feedURI = aFeedURI;
  },

  writeSiteURI(aSiteURI, aSource) {
    if (!aSiteURI) {
      PlacesUtils.annotations.removeItemAnnotation(this.id,
                                                   PlacesUtils.LMANNO_SITEURI,
                                                   aSource);
      this.siteURI = null;
      return;
    }

    // Security check the site URI against the feed URI principal.
    let secMan = Services.scriptSecurityManager;
    let feedPrincipal = secMan.createCodebasePrincipal(this.feedURI, {});
    try {
      secMan.checkLoadURIWithPrincipal(feedPrincipal, aSiteURI,
                                       Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
    } catch (ex) {
      return;
    }

    PlacesUtils.annotations
               .setItemAnnotation(this.id, PlacesUtils.LMANNO_SITEURI,
                                  aSiteURI.spec,
                                  0, PlacesUtils.annotations.EXPIRE_NEVER,
                                  aSource, true);
    this.siteURI = aSiteURI;
  },

  /**
   * Tries to updates the livemark if needed.
   * The update process is asynchronous.
   *
   * @param [optional] aForceUpdate
   *        If true will try to update the livemark even if its contents have
   *        not yet expired.
   */
  updateChildren(aForceUpdate) {
    // Check if the livemark is already updating.
    if (this.status == Ci.mozILivemark.STATUS_LOADING)
      return;

    // Check the TTL/expiration on this, to check if there is no need to update
    // this livemark.
    if (!aForceUpdate && this.children.length && this.expireTime > Date.now())
      return;

    this.status = Ci.mozILivemark.STATUS_LOADING;

    // Setting the status notifies observers that may remove the livemark.
    if (this._terminated)
      return;

    try {
      // Create a load group for the request.  This will allow us to
      // automatically keep track of redirects, so we can always
      // cancel the channel.
      let loadgroup = Cc["@mozilla.org/network/load-group;1"].
                      createInstance(Ci.nsILoadGroup);
      // Creating a CodeBasePrincipal and using it as the loadingPrincipal
      // is *not* desired and is only tolerated within this file.
      // TODO: Find the right OriginAttributes and pass something other
      // than {} to .createCodeBasePrincipal().
      let channel = NetUtil.newChannel({
        uri: this.feedURI,
        loadingPrincipal: Services.scriptSecurityManager.createCodebasePrincipal(this.feedURI, {}),
        securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_XMLHTTPREQUEST
      }).QueryInterface(Ci.nsIHttpChannel);
      channel.loadGroup = loadgroup;
      channel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND |
                           Ci.nsIRequest.LOAD_BYPASS_CACHE;
      channel.requestMethod = "GET";
      channel.setRequestHeader("X-Moz", "livebookmarks", false);

      // Stream the result to the feed parser with this listener
      let listener = new LivemarkLoadListener(this);
      channel.notificationCallbacks = listener;
      channel.asyncOpen2(listener);

      this.loadGroup = loadgroup;
    } catch (ex) {
      this.status = Ci.mozILivemark.STATUS_FAILED;
    }
  },

  reload(aForceUpdate) {
    this.updateChildren(aForceUpdate);
  },

  get children() {
    return this._children;
  },
  set children(val) {
    this._children = val;

    // Discard the previous cached nodes, new ones should be generated.
    for (let container of this._resultObservers.keys()) {
      this._nodes.delete(container);
    }

    // Update visited status for each entry.
    for (let child of this._children) {
      history.hasVisits(child.uri).then(isVisited => {
        this.updateURIVisitedStatus(child.uri, isVisited);
      }).catch(Cu.reportError);
    }

    return this._children;
  },

  _isURIVisited(aURI) {
    return this.children.some(child => child.uri.equals(aURI) && child.visited);
  },

  getNodesForContainer(aContainerNode) {
    if (this._nodes.has(aContainerNode)) {
      return this._nodes.get(aContainerNode);
    }

    let livemark = this;
    let nodes = [];
    let now = Date.now() * 1000;
    for (let child of this.children) {
      let node = {
        // The QueryInterface is needed cause aContainerNode is a jsval.
        // This is required to avoid issues with scriptable wrappers that would
        // not allow the view to correctly set expandos.
        get parent() {
          return aContainerNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
        },
        get parentResult() {
          return this.parent.parentResult;
        },
        get uri() {
          return child.uri.spec;
        },
        get type() {
          return Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;
        },
        get title() {
          return child.title;
        },
        get accessCount() {
          return Number(livemark._isURIVisited(NetUtil.newURI(this.uri)));
        },
        get time() {
          return 0;
        },
        get icon() {
          return "";
        },
        get indentLevel() {
          return this.parent.indentLevel + 1;
        },
        get bookmarkIndex() {
            return -1;
        },
        get itemId() {
            return -1;
        },
        get dateAdded() {
          return now;
        },
        get lastModified() {
          return now;
        },
        get tags() {
          return PlacesUtils.tagging.getTagsForURI(NetUtil.newURI(this.uri)).join(", ");
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsINavHistoryResultNode])
      };
      nodes.push(node);
    }
    this._nodes.set(aContainerNode, nodes);
    return nodes;
  },

  registerForUpdates(aContainerNode, aResultObserver) {
    this._resultObservers.set(aContainerNode, aResultObserver);
  },

  unregisterForUpdates(aContainerNode) {
    this._resultObservers.delete(aContainerNode);
    this._nodes.delete(aContainerNode);
  },

  _invalidateRegisteredContainers() {
    for (let [ container, observer ] of this._resultObservers) {
      observer.invalidateContainer(container);
    }
  },

  /**
   * Updates the visited status of nodes observing this livemark.
   *
   * @param href
   *        If provided will update nodes having the given uri,
   *        otherwise any node.
   * @param visitedStatus
   *        Whether the nodes should be set as visited.
   */
  updateURIVisitedStatus(href, visitedStatus) {
    let wasVisited = false;
    for (let child of this.children) {
      if (!href || child.uri.spec == href) {
        wasVisited = child.visited;
        child.visited = visitedStatus;
      }
    }

    for (let [ container, observer ] of this._resultObservers) {
      if (this._nodes.has(container)) {
        let nodes = this._nodes.get(container);
        for (let node of nodes) {
          if (!href || node.uri == href) {
            Services.tm.dispatchToMainThread(() => {
              observer.nodeHistoryDetailsChanged(node, node.time, wasVisited);
            });
          }
        }
      }
    }
  },

  /**
   * Terminates the livemark entry, cancelling any ongoing load.
   * Must be invoked before destroying the entry.
   */
  terminate() {
    // Avoid handling any updateChildren request from now on.
    this._terminated = true;
    this.abort();
  },

  /**
   * Aborts the livemark loading if needed.
   */
  abort() {
    this.status = Ci.mozILivemark.STATUS_FAILED;
    if (this.loadGroup) {
      this.loadGroup.cancel(Cr.NS_BINDING_ABORTED);
      this.loadGroup = null;
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.mozILivemark
  ])
};

// LivemarkLoadListener

/**
 * Object used internally to handle loading a livemark's contents.
 *
 * @param aLivemark
 *        The Livemark that is loading.
 */
function LivemarkLoadListener(aLivemark) {
  this._livemark = aLivemark;
  this._processor = null;
  this._isAborted = false;
  this._ttl = EXPIRE_TIME_MS;
}

LivemarkLoadListener.prototype = {
  abort(aException) {
    if (!this._isAborted) {
      this._isAborted = true;
      this._livemark.abort();
      this._setResourceTTL(ONERROR_EXPIRE_TIME_MS);
    }
  },

  // nsIFeedResultListener
  handleResult(aResult) {
    if (this._isAborted) {
      return;
    }

    try {
      // We need this to make sure the item links are safe
      let feedPrincipal =
        Services.scriptSecurityManager
                .createCodebasePrincipal(this._livemark.feedURI, {});

      // Enforce well-formedness because the existing code does
      if (!aResult || !aResult.doc || aResult.bozo) {
        throw new Components.Exception("", Cr.NS_ERROR_FAILURE);
      }

      let feed = aResult.doc.QueryInterface(Ci.nsIFeed);
      let siteURI = this._livemark.siteURI;
      if (feed.link && (!siteURI || !feed.link.equals(siteURI))) {
        siteURI = feed.link;
        this._livemark.writeSiteURI(siteURI);
      }

      // Insert feed items.
      let livemarkChildren = [];
      for (let i = 0; i < feed.items.length; ++i) {
        let entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
        let uri = entry.link || siteURI;
        if (!uri) {
          continue;
        }

        try {
          Services.scriptSecurityManager
                  .checkLoadURIWithPrincipal(feedPrincipal, uri,
                                             Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
        } catch (ex) {
          continue;
        }

        let title = entry.title ? entry.title.plainText() : "";
        livemarkChildren.push({ uri, title, visited: false });
      }

      this._livemark.children = livemarkChildren;
    } catch (ex) {
      Cu.reportError(ex);
      this.abort(ex);
    } finally {
      this._processor.listener = null;
      this._processor = null;
    }
  },

  onDataAvailable(aRequest, aContext, aInputStream, aSourceOffset, aCount) {
    if (this._processor) {
      this._processor.onDataAvailable(aRequest, aContext, aInputStream,
                                      aSourceOffset, aCount);
    }
  },

  onStartRequest(aRequest, aContext) {
    if (this._isAborted) {
      throw new Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    try {
      // Parse feed data as it comes in
      this._processor = Cc["@mozilla.org/feed-processor;1"].
                        createInstance(Ci.nsIFeedProcessor);
      this._processor.listener = this;
      this._processor.parseAsync(null, channel.URI);
      this._processor.onStartRequest(aRequest, aContext);
    } catch (ex) {
      Cu.reportError("Livemark Service: feed processor received an invalid channel for " + channel.URI.spec);
      this.abort(ex);
    }
  },

  onStopRequest(aRequest, aContext, aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      this.abort();
      return;
    }

    // Set an expiration on the livemark, to reloading the data in future.
    try {
      if (this._processor) {
        this._processor.onStopRequest(aRequest, aContext, aStatus);
      }

      // Calculate a new ttl
      let channel = aRequest.QueryInterface(Ci.nsICachingChannel);
      if (channel) {
        let entryInfo = channel.cacheToken.QueryInterface(Ci.nsICacheEntry);
        if (entryInfo) {
          // nsICacheEntry returns value as seconds.
          let expireTime = entryInfo.expirationTime * 1000;
          let nowTime = Date.now();
          // Note, expireTime can be 0, see bug 383538.
          if (expireTime > nowTime) {
            this._setResourceTTL(Math.max((expireTime - nowTime),
                                          EXPIRE_TIME_MS));
            return;
          }
        }
      }
      this._setResourceTTL(EXPIRE_TIME_MS);
    } catch (ex) {
      this.abort(ex);
    } finally {
      if (this._livemark.status == Ci.mozILivemark.STATUS_LOADING) {
        this._livemark.status = Ci.mozILivemark.STATUS_READY;
      }
      this._livemark.locked = false;
      this._livemark.loadGroup = null;
    }
  },

  _setResourceTTL(aMilliseconds) {
    this._livemark.expireTime = Date.now() + aMilliseconds;
  },

  // nsIInterfaceRequestor
  getInterface(aIID) {
    return this.QueryInterface(aIID);
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIFeedResultListener,
    Ci.nsIStreamListener,
    Ci.nsIRequestObserver,
    Ci.nsIInterfaceRequestor
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([LivemarkService]);
