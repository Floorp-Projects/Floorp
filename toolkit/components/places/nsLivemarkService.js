/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

////////////////////////////////////////////////////////////////////////////////
//// Modules

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Services

XPCOMUtils.defineLazyServiceGetter(this, "secMan",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");
XPCOMUtils.defineLazyGetter(this, "asyncHistory", function () {
  // Lazily add an history observer when it's actually needed.
  PlacesUtils.history.addObserver(PlacesUtils.livemarks, true);
  return Cc["@mozilla.org/browser/history;1"].getService(Ci.mozIAsyncHistory);
});

////////////////////////////////////////////////////////////////////////////////
//// Constants

// Security flags for checkLoadURIWithPrincipal.
const SEC_FLAGS = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;

// Delay between reloads of consecute livemarks.
const RELOAD_DELAY_MS = 500;
// Expire livemarks after this time.
const EXPIRE_TIME_MS = 3600000; // 1 hour.
// Expire livemarks after this time on error.
const ONERROR_EXPIRE_TIME_MS = 300000; // 5 minutes.

////////////////////////////////////////////////////////////////////////////////
//// LivemarkService

function LivemarkService()
{
  // Cleanup on shutdown.
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN, true);
 
  // Observe bookmarks and history, but don't init the services just for that.
  PlacesUtils.addLazyBookmarkObserver(this, true);

  // Asynchronously build the livemarks cache.
  this._ensureAsynchronousCache();
}

LivemarkService.prototype = {
  // Cache of Livemark objects, hashed by bookmarks folder ids.
  _livemarks: {},
  // Hash associating guids to bookmarks folder ids.
  _guids: {},

  get _populateCacheSQL()
  {
    function getAnnoSQLFragment(aAnnoParam) {
      return "SELECT a.content "
           + "FROM moz_items_annos a "
           + "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
           + "WHERE a.item_id = b.id "
           +   "AND n.name = " + aAnnoParam;
    }

    return "SELECT b.id, b.title, b.parent, b.position, b.guid, b.lastModified, "
         +        "(" + getAnnoSQLFragment(":feedURI_anno") + ") AS feedURI, "
         +        "(" + getAnnoSQLFragment(":siteURI_anno") + ") AS siteURI "
         + "FROM moz_bookmarks b "
         + "JOIN moz_items_annos a ON a.item_id = b.id "
         + "JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id "
         + "WHERE b.type = :folder_type "
         +   "AND n.name = :feedURI_anno ";
  },

  _ensureAsynchronousCache: function LS__ensureAsynchronousCache()
  {
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                .DBConnection;
    let stmt = db.createAsyncStatement(this._populateCacheSQL);
    stmt.params.folder_type = Ci.nsINavBookmarksService.TYPE_FOLDER;
    stmt.params.feedURI_anno = PlacesUtils.LMANNO_FEEDURI;
    stmt.params.siteURI_anno = PlacesUtils.LMANNO_SITEURI;

    let livemarkSvc = this;
    this._pendingStmt = stmt.executeAsync({
      handleResult: function LS_handleResult(aResults)
      {
        for (let row = aResults.getNextRow(); row; row = aResults.getNextRow()) {
          let id = row.getResultByName("id");
          let siteURL = row.getResultByName("siteURI");
          let guid = row.getResultByName("guid");
          livemarkSvc._livemarks[id] =
            new Livemark({ id: id,
                           guid: guid,             
                           title: row.getResultByName("title"),
                           parentId: row.getResultByName("parent"),
                           index: row.getResultByName("position"),
                           lastModified: row.getResultByName("lastModified"),
                           feedURI: NetUtil.newURI(row.getResultByName("feedURI")),
                           siteURI: siteURL ? NetUtil.newURI(siteURL) : null,
            });
          livemarkSvc._guids[guid] = id;
        }
      },
      handleError: function LS_handleError(aErr)
      {
        Cu.reportError("AsyncStmt error (" + aErr.result + "): '" + aErr.message);
      },
      handleCompletion: function LS_handleCompletion() {
        livemarkSvc._pendingStmt = null;
      }
    });
    stmt.finalize();
  },

  _onCacheReady: function LS__onCacheReady(aCallback, aWaitForAsyncWrites)
  {
    if (this._pendingStmt || aWaitForAsyncWrites) {
      // The cache is still being populated, so enqueue the job to the Storage
      // async thread.  Ideally this should just dispatch a runnable to it,
      // that would call back on the main thread, but bug 608142 made that
      // impossible.  Thus just enqueue the cheapest query possible.
      let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                  .DBConnection;
      let stmt = db.createAsyncStatement("PRAGMA encoding");
      stmt.executeAsync({
        handleError: function () {},
        handleResult: function () {},
        handleCompletion: function ETAT_handleCompletion()
        {
          aCallback();
        }
      });
      stmt.finalize();
    }
    else {
      // The callbacks should always be enqueued per the interface.
      // Just enque on the main thread.
      Services.tm.mainThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
    }
  },

  _reloading: false,
  _startReloadTimer: function LS__startReloadTimer()
  {
    if (this._reloadTimer) {
      this._reloadTimer.cancel();
    }
    else {
      this._reloadTimer = Cc["@mozilla.org/timer;1"]
                            .createInstance(Ci.nsITimer);
    }
    this._reloading = true;
    this._reloadTimer.initWithCallback(this._reloadNextLivemark.bind(this),
                                       RELOAD_DELAY_MS,
                                       Ci.nsITimer.TYPE_ONE_SHOT);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function LS_observe(aSubject, aTopic, aData)
  {
    if (aTopic == PlacesUtils.TOPIC_SHUTDOWN) {
      if (this._pendingStmt) {
        this._pendingStmt.cancel();
        this._pendingStmt = null;
        // Initialization never finished, so just bail out.
        return;
      }

      if (this._reloadTimer) {
        this._reloading = false;
        this._reloadTimer.cancel();
        delete this._reloadTimer;
      }

      // Stop any ongoing update.
      for each (let livemark in this._livemarks) {
        livemark.terminate();
      }
      this._livemarks = {};
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Deprecated nsILivemarkService methods

  _reportDeprecatedMethod: function DEPRECATED_LS__reportDeprecatedMethod()
  {
    let oldFuncName = arguments.callee.caller.name.slice(14);
    Cu.reportError(oldFuncName + " is deprecated and will be removed in a " +
                   "future release.  Check the nsILivemarkService interface.");
  },

  _ensureSynchronousCache: function DEPRECATED_LS__ensureSynchronousCache()
  {
    if (!this._pendingStmt) {
      return;
    }
    this._pendingStmt.cancel();
    this._pendingStmt = null;

    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                .DBConnection;
    let stmt = db.createStatement(this._populateCacheSQL);
    stmt.params.folder_type = Ci.nsINavBookmarksService.TYPE_FOLDER;
    stmt.params.feedURI_anno = PlacesUtils.LMANNO_FEEDURI;
    stmt.params.siteURI_anno = PlacesUtils.LMANNO_SITEURI;

    while (stmt.executeStep()) {
      let id = stmt.row.id;
      let siteURL = stmt.row.siteURI;
      let guid = stmt.row.guid;
      this._livemarks[id] =
        new Livemark({ id: id,
                       guid: guid,
                       title: stmt.row.title,
                       parentId: stmt.row.parent,
                       index: stmt.row.position,
                       lastModified: stmt.row.lastModified,
                       feedURI: NetUtil.newURI(stmt.row.feedURI),
                       siteURI: siteURL ? NetUtil.newURI(siteURL) : null,
        });
      this._guids[guid] = id;
    }
    stmt.finalize();
  },

  start: function DEPRECATED_LS_start()
  {
    this._reportDeprecatedMethod();
  },

  stopUpdateLivemarks: function DEPRECATED_LS_stopUpdateLivemarks()
  {
    this._reportDeprecatedMethod();
  },

  createLivemark: function LS_createLivemark(aParentId, aTitle, aSiteURI,
                                             aFeedURI, aIndex)
  {
    this._reportDeprecatedMethod();
    this._ensureSynchronousCache();

    if (!aParentId || aParentId < 1 ||
        aIndex < -1 ||
        !aFeedURI || !(aFeedURI instanceof Ci.nsIURI) ||
        (aSiteURI && !(aSiteURI instanceof Ci.nsIURI)) ||
         aParentId in this._livemarks) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let livemark = new Livemark({ title:    aTitle
                                , parentId: aParentId
                                , index:    aIndex
                                , feedURI:  aFeedURI
                                , siteURI:  aSiteURI
                                });
    if (this._itemAdded && this._itemAdded.id == livemark.id) {
      livemark.guid = this._itemAdded.guid;
      livemark.index = this._itemAdded.index;
      livemark.lastModified = this._itemAdded.lastModified;
    }

    this._livemarks[livemark.id] = livemark;
    this._guids[livemark.guid] = livemark.id;
    return livemark.id;
  },

  createLivemarkFolderOnly: function DEPRECATED_LS_(aParentId, aTitle, aSiteURI,
                                                    aFeedURI, aIndex)
  {
    return this.createLivemark(aParentId, aTitle, aSiteURI, aFeedURI, aIndex);
  },

  isLivemark: function DEPRECATED_LS_isLivemark(aId)
  {
    this._reportDeprecatedMethod();
    if (!aId || aId < 1) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._ensureSynchronousCache();

    return aId in this._livemarks;
  },

  getLivemarkIdForFeedURI:
  function DEPRECATED_LS_getLivemarkIdForFeedURI(aFeedURI)
  {
    this._reportDeprecatedMethod();
    if (!(aFeedURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._ensureSynchronousCache();

    for each (livemark in this._livemarks) {
      if (livemark.feedURI.equals(aFeedURI)) {
        return livemark.id;
      }
    }
    return -1;
  },

  getSiteURI: function DEPRECATED_LS_getSiteURI(aId)
  {
    this._reportDeprecatedMethod();
    this._ensureSynchronousCache();

    if (!(aId in this._livemarks)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    return this._livemarks[aId].siteURI;
  },

  setSiteURI: function DEPRECATED_LS_writeSiteURI(aId, aSiteURI)
  {
    this._reportDeprecatedMethod();
    if (!aSiteURI || !(aSiteURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
  },

  getFeedURI: function DEPRECATED_LS_getFeedURI(aId)
  {
    this._reportDeprecatedMethod();
    this._ensureSynchronousCache();

    if (!(aId in this._livemarks)) {
      throw Cr.NS_ERROR_INVALID_ARG;      
    }
    return this._livemarks[aId].feedURI;
  },

  setFeedURI: function DEPRECATED_LS_writeFeedURI(aId, aFeedURI)
  {
    this._reportDeprecatedMethod();
    if (!aFeedURI || !(aFeedURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
  },

  reloadLivemarkFolder: function DEPRECATED_LS_reloadLivemarkFolder(aId)
  {
    this._reportDeprecatedMethod();
    this._ensureSynchronousCache();

    if (!(aId in this._livemarks)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._livemarks[aId].reload();
  },

  reloadAllLivemarks: function DEPRECATED_LS_reloadAllLivemarks()
  {
    this._reportDeprecatedMethod();

    this._reloadLivemarks(true);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIAsyncLivemarks

  addLivemark: function LS_addLivemark(aLivemarkInfo,
                                       aLivemarkCallback)
  {
    // Must provide at least non-null parentId, index and feedURI.
    if (!aLivemarkInfo ||
        ("parentId" in aLivemarkInfo && aLivemarkInfo.parentId < 1) ||
        !("index" in aLivemarkInfo) || aLivemarkInfo.index < Ci.nsINavBookmarksService.DEFAULT_INDEX ||
        !(aLivemarkInfo.feedURI instanceof Ci.nsIURI) ||
        (aLivemarkInfo.siteURI && !(aLivemarkInfo.siteURI instanceof Ci.nsIURI)) ||
        (aLivemarkInfo.guid && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid))) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    // The addition is done synchronously due to the fact importExport service
    // and JSON backups require that.  The notification is async though.
    // Once bookmarks are async, this may be properly fixed.
    let result = Cr.NS_OK;
    let livemark = null;
    try {
      // Disallow adding a livemark inside another livemark.
      if (aLivemarkInfo.parentId in this._livemarks) {
        throw new Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
      }

      // Don't pass unexpected input data to the livemark constructor.
      livemark = new Livemark({ title:        aLivemarkInfo.title
                              , parentId:     aLivemarkInfo.parentId
                              , index:        aLivemarkInfo.index
                              , feedURI:      aLivemarkInfo.feedURI
                              , siteURI:      aLivemarkInfo.siteURI
                              , guid:         aLivemarkInfo.guid
                              , lastModified: aLivemarkInfo.lastModified
                              });
      if (this._itemAdded && this._itemAdded.id == livemark.id) {
        livemark.index = this._itemAdded.index;
        if (!aLivemarkInfo.guid) {
          livemark.guid = this._itemAdded.guid;
        }
        if (!aLivemarkInfo.lastModified) {
          livemark.lastModified = this._itemAdded.lastModified;
        }
      }

      // Updating the cache even if it has not yet been populated doesn't
      // matter since it will just be overwritten.  But it must be done now,
      // otherwise checking for the livemark using any deprecated synchronous
      // API from an onItemAnnotation notification would give a wrong result.
      this._livemarks[livemark.id] = livemark;
      this._guids[aLivemarkInfo.guid] = livemark.id;    
    }
    catch (ex) {
      result = ex.result;
      livemark = null;
    }
    finally {
      if (aLivemarkCallback) {
        this._onCacheReady(function LS_addLivemark_ETAT() {
          try {
            aLivemarkCallback.onCompletion(result, livemark);
          } catch(ex2) {}
        }, true);
      }
    }
  },

  removeLivemark: function LS_removeLivemark(aLivemarkInfo, aLivemarkCallback)
  {
    if (!aLivemarkInfo) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    // Accept either a guid or an id.
    let id = aLivemarkInfo.guid || aLivemarkInfo.id;
    if (("guid" in aLivemarkInfo && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid)) ||
        ("id" in aLivemarkInfo && aLivemarkInfo.id < 1) ||
        !id) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    // Convert the guid to an id.
    if (id in this._guids) {
      id = this._guids[id];
    }
    let result = Cr.NS_OK;
    try {
      if (!(id in this._livemarks)) {
        throw new Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
      }
      this._livemarks[id].remove();
    }
    catch (ex) {
      result = ex.result;
    }
    finally {
      if (aLivemarkCallback) {
        // Enqueue the notification, per interface definition.
        this._onCacheReady(function LS_removeLivemark_ETAT() {
          try {
            aLivemarkCallback.onCompletion(result, null);
          } catch(ex2) {}
        });
      }
    }
  },

  _reloaded: [],
  _reloadNextLivemark: function LS__reloadNextLivemark()
  {
    this._reloading = false;
    // Find first livemark to be reloaded.
    for (let id in this._livemarks) {
      if (this._reloaded.indexOf(id) == -1) {
        this._reloaded.push(id);
        this._livemarks[id].reload(this._forceUpdate);
        this._startReloadTimer();
        break;
      }
    }
  },

  reloadLivemarks: function LS_reloadLivemarks(aForceUpdate)
  {
    // Check if there's a currently running reload, to save some useless work.
    let notWorthRestarting =
      this._forceUpdate || // We're already forceUpdating.
      !aForceUpdate;       // The caller didn't request a forced update.
    if (this._reloading && notWorthRestarting) {
      // Ignore this call.
      return;
    } 

    this._onCacheReady((function LS_reloadAllLivemarks_ETAT() {
      this._forceUpdate = !!aForceUpdate;
      this._reloaded = [];
      // Livemarks reloads happen on a timer, and are delayed for performance
      // reasons.
      this._startReloadTimer();
    }).bind(this));
  },

  getLivemark: function LS_getLivemark(aLivemarkInfo, aLivemarkCallback)
  {
    if (!aLivemarkInfo) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    // Accept either a guid or an id.
    let id = aLivemarkInfo.guid || aLivemarkInfo.id;
    if (("guid" in aLivemarkInfo && !/^[a-zA-Z0-9\-_]{12}$/.test(aLivemarkInfo.guid)) ||
        ("id" in aLivemarkInfo && aLivemarkInfo.id < 1) ||
        !id) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    this._onCacheReady((function LS_getLivemark_ETAT() {
      // Convert the guid to an id.
      if (id in this._guids) {
        id = this._guids[id];
      }
      if (id in this._livemarks) {
        try {
          aLivemarkCallback.onCompletion(Cr.NS_OK, this._livemarks[id]);
        } catch (ex) {}
      }
      else {
        try {
          aLivemarkCallback.onCompletion(Cr.NS_ERROR_INVALID_ARG, null);
        } catch (ex) {}
      }
    }).bind(this));
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsINavBookmarkObserver

  onBeginUpdateBatch:  function () {},
  onEndUpdateBatch:    function () {},
  onItemVisited:       function () {},
  onBeforeItemRemoved: function () {},

  _itemAdded: null,
  onItemAdded: function LS_onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                       aURI, aTitle, aDateAdded, aGUID)
  {
    if (aItemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      this._itemAdded = { id: aItemId
                        , guid: aGUID
                        , index: aIndex
                        , lastModified: aDateAdded
                        };
    }
  },

  onItemChanged: function LS_onItemChanged(aItemId, aProperty, aIsAnno, aValue,
                                           aLastModified, aItemType)
  {
    if (aItemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      if (this._itemAdded && this._itemAdded.id == aItemId) {
        this._itemAdded.lastModified = aLastModified;
     }
      if (aItemId in this._livemarks) {
        if (aProperty == "title") {
          this._livemarks[aItemId].title = aValue;
        }
        this._livemarks[aItemId].lastModified = aLastModified;
      }
    }
  },

  onItemMoved: function LS_onItemMoved(aItemId, aOldParentId, aOldIndex,
                                      aNewParentId, aNewIndex, aItemType)
  {
    if (aItemType == Ci.nsINavBookmarksService.TYPE_FOLDER &&
        aItemId in this._livemarks) {
      this._livemarks[aItemId].parentId = aNewParentId;
      this._livemarks[aItemId].index = aNewIndex;
    }
  },

  onItemRemoved: function LS_onItemRemoved(aItemId, aParentId, aIndex,
                                           aItemType, aURI, aGUID)
  {
    if (aItemType == Ci.nsINavBookmarksService.TYPE_FOLDER &&
        aItemId in this._livemarks) {
      this._livemarks[aItemId].terminate();
      delete this._livemarks[aItemId];
      delete this._guids[aGUID];
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsINavHistoryObserver

  onBeginUpdateBatch: function () {},
  onEndUpdateBatch:   function () {},
  onPageChanged:      function () {},
  onTitleChanged:     function () {},
  onDeleteVisits:     function () {},
  onBeforeDeleteURI:  function () {},

  onClearHistory:     function () {},

  onDeleteURI: function PS_onDeleteURI(aURI) {
    for each (let livemark in this._livemarks) {
      livemark.updateURIVisitedStatus(aURI, false);
    }
  },

  onVisit: function PS_onVisit(aURI) {
    for each (let livemark in this._livemarks) {
      livemark.updateURIVisitedStatus(aURI, true);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("{dca61eb5-c7cd-4df1-b0fb-d0722baba251}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(LivemarkService),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsILivemarkService
  , Ci.mozIAsyncLivemarks
  , Ci.nsINavBookmarkObserver
  , Ci.nsINavHistoryObserver
  , Ci.nsIObserver
  , Ci.nsISupportsWeakReference
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// Livemark

/**
 * Object used internally to represent a livemark.
 *
 * @param aLivemarkInfo
 *        Object containing information on the livemark.  If the livemark is
 *        not included in the object, a new livemark will be created.
 *
 * @note terminate() must be invoked before getting rid of this object.
 */
function Livemark(aLivemarkInfo)
{
  this.title = aLivemarkInfo.title;
  this.parentId = aLivemarkInfo.parentId;
  this.index = aLivemarkInfo.index;

  this._status = Ci.mozILivemark.STATUS_READY;

  // Hash of resultObservers, hashed by container.
  this._resultObservers = new WeakMap();
  // This keeps a list of the containers used as keys in the weakmap, since
  // it's not iterable.  In future may use an iterable Map.
  this._resultObserversList = [];

  // Sorted array of objects representing livemark children in the form
  // { uri, title, visited }.
  this._children = [];

  // Keeps a separate array of nodes for each requesting container, hashed by
  // the container itself.
  this._nodes = new WeakMap();

  this._guid = "";
  this._lastModified = 0;

  this.loadGroup = null;
  this.feedURI = null;
  this.siteURI = null;
  this.expireTime = 0;

  if (aLivemarkInfo.id) {
    // This request comes from the cache.
    this.id = aLivemarkInfo.id;
    this.guid = aLivemarkInfo.guid;
    this.feedURI = aLivemarkInfo.feedURI;
    this.siteURI = aLivemarkInfo.siteURI;
    this.lastModified = aLivemarkInfo.lastModified;
  }
  else {
    // Create a new livemark.
    this.id = PlacesUtils.bookmarks.createFolder(aLivemarkInfo.parentId,
                                                 aLivemarkInfo.title,
                                                 aLivemarkInfo.index);
    PlacesUtils.bookmarks.setFolderReadonly(this.id, true);
    if (aLivemarkInfo.guid) {
      this.writeGuid(aLivemarkInfo.guid);
    }
    this.writeFeedURI(aLivemarkInfo.feedURI);
    if (aLivemarkInfo.siteURI) {
      this.writeSiteURI(aLivemarkInfo.siteURI);
    }
    // Last modified time must be the last change.
    if (aLivemarkInfo.lastModified) {
      this.lastModified = aLivemarkInfo.lastModified;
      PlacesUtils.bookmarks.setItemLastModified(this.id, this.lastModified);
    }
  }
}

Livemark.prototype = {
  get status() this._status,
  set status(val) {
    if (this._status != val) {
      this._status = val;
      this._invalidateRegisteredContainers();
    }
    return this._status;
  },

  /**
   * Sets an annotation on the bookmarks folder id representing the livemark.
   *
   * @param aAnnoName
   *        Name of the annotation.
   * @param aValue
   *        Value of the annotation.
   * @return The annotation value.
   * @throws If the folder is invalid.
   */
  _setAnno: function LM__setAnno(aAnnoName, aValue)
  {
    PlacesUtils.annotations
               .setItemAnnotation(this.id, aAnnoName, aValue, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER);
  },

  writeFeedURI: function LM_writeFeedURI(aFeedURI)
  {
    this._setAnno(PlacesUtils.LMANNO_FEEDURI, aFeedURI.spec);
    this.feedURI = aFeedURI;
  },

  writeSiteURI: function LM_writeSiteURI(aSiteURI)
  {
    if (!aSiteURI) {
      PlacesUtils.annotations.removeItemAnnotation(this.id,
                                                   PlacesUtils.LMANNO_SITEURI)
      this.siteURI = null;
      return;
    }

    // Security check the site URI against the feed URI principal.
    let feedPrincipal = secMan.getCodebasePrincipal(this.feedURI);
    try {
      secMan.checkLoadURIWithPrincipal(feedPrincipal, aSiteURI, SEC_FLAGS);
    }
    catch (ex) {
      return;
    }

    this._setAnno(PlacesUtils.LMANNO_SITEURI, aSiteURI.spec)
    this.siteURI = aSiteURI;
  },

  writeGuid: function LM_writeGuid(aGUID)
  {
    // There isn't a way to create a bookmark with a given guid yet, nor to
    // set a guid on an existing one.  So, for now, just go the dirty way.
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                .DBConnection;
    let stmt = db.createAsyncStatement("UPDATE moz_bookmarks " +
                                       "SET guid = :guid " +
                                       "WHERE id = :item_id");
    stmt.params.guid = aGUID;
    stmt.params.item_id = this.id;
    let livemark = this;
    stmt.executeAsync({
      handleError: function () {},
      handleResult: function () {},
      handleCompletion: function ETAT_handleCompletion(aReason)
      {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          livemark._guid = aGUID;
        }
      }
    });
    stmt.finalize();
  },

  set guid(aGUID) {
    this._guid = aGUID;
    return aGUID;
  },
  get guid()
  {
    if (!/^[a-zA-Z0-9\-_]{12}$/.test(this._guid)) {
      // The old synchronous interface is not guid-aware, so this may still have
      // to be populated manually.
      let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                  .DBConnection;
      let stmt = db.createStatement("SELECT guid FROM moz_bookmarks " +
                                    "WHERE id = :item_id");
      stmt.params.item_id = this.id;
      try {
        if (stmt.executeStep()) {
          this._guid = stmt.row.guid;
        }
      } finally {
        stmt.finalize();
      }
    }
    return this._guid;
  },

  set lastModified(aLastModified) {
    this._lastModified = aLastModified;
    return aLastModified;
  },
  get lastModified()
  {
    if (!this._lastModified) {
      // The old synchronous interface ignores the last modified time.
      let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                  .DBConnection;
      let stmt = db.createStatement("SELECT lastModified FROM moz_bookmarks " +
                                    "WHERE id = :item_id");
      stmt.params.item_id = this.id;
      try {
        if (stmt.executeStep()) {
          this._lastModified = stmt.row.lastModified;
        }
      } finally {
        stmt.finalize();
      }
    }
    return this._lastModified;
  },

  /**
   * Tries to updates the livemark if needed.
   * The update process is asynchronous.
   *
   * @param [optional] aForceUpdate
   *        If true will try to update the livemark even if its contents have
   *        not yet expired.
   */
  updateChildren: function LM_updateChildren(aForceUpdate)
  {
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
      let channel = NetUtil.newChannel(this.feedURI.spec).
                    QueryInterface(Ci.nsIHttpChannel);
      channel.loadGroup = loadgroup;
      channel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND |
                           Ci.nsIRequest.LOAD_BYPASS_CACHE;
      channel.requestMethod = "GET";
      channel.setRequestHeader("X-Moz", "livebookmarks", false);

      // Stream the result to the feed parser with this listener
      let listener = new LivemarkLoadListener(this);
      channel.notificationCallbacks = listener;
      channel.asyncOpen(listener, null);

      this.loadGroup = loadgroup;
    }
    catch (ex) {
      this.status = Ci.mozILivemark.STATUS_FAILED;
    }
  },

  reload: function LM_reload(aForceUpdate)
  {
    this.updateChildren(aForceUpdate);
  },

  remove: function LM_remove() {
    PlacesUtils.bookmarks.removeItem(this.id);
  },

  get children() this._children,
  set children(val) {
    this._children = val;

    // Discard the previous cached nodes, new ones should be generated.
    for (let i = 0; i < this._resultObserversList.length; i++) {
      let container = this._resultObserversList[i];
      this._nodes.delete(container);
    }

    // Update visited status for each entry.
    for (let i = 0; i < this._children.length; i++) {
      let child = this._children[i];
      asyncHistory.isURIVisited(child.uri,
        (function(aURI, aIsVisited) {
          this.updateURIVisitedStatus(aURI, aIsVisited);
        }).bind(this));
    }

    return this._children;
  },

  _isURIVisited: function LM__isURIVisited(aURI) {
    for (let i = 0; i < this.children.length; i++) {
      if (this.children[i].uri.equals(aURI)) {
        return this.children[i].visited;
      }
    }
  },

  getNodesForContainer: function LM_getNodesForContainer(aContainerNode)
  {
    if (this._nodes.has(aContainerNode)) {
      return this._nodes.get(aContainerNode);
    }

    let livemark = this;
    let nodes = [];
    let now = Date.now() * 1000;
    for (let i = 0; i < this._children.length; i++) {
      let child = this._children[i];
      let node = {
        // The QueryInterface is needed cause aContainerNode is a jsval.
        // This is required to avoid issues with scriptable wrappers that would
        // not allow the view to correctly set expandos.
        get parent()
          aContainerNode.QueryInterface(Ci.nsINavHistoryContainerResultNode),
        get parentResult() this.parent.parentResult,
        get uri() child.uri.spec,
        get type() Ci.nsINavHistoryResultNode.RESULT_TYPE_URI,
        get title() child.title,
        get accessCount()
          Number(livemark._isURIVisited(NetUtil.newURI(this.uri))),
        get time() 0,
        get icon() "",
        get indentLevel() this.parent.indentLevel + 1,
        get bookmarkIndex() -1,
        get itemId() -1,
        get dateAdded() now + i,
        get lastModified() now + i,
        get tags()
          PlacesUtils.tagging.getTagsForURI(NetUtil.newURI(this.uri)).join(", "),
        QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryResultNode])
      };
      nodes.push(node);
    }
    this._nodes.set(aContainerNode, nodes);
    return nodes;
  },

  registerForUpdates: function LM_registerForUpdates(aContainerNode,
                                                     aResultObserver)
  {
    this._resultObservers.set(aContainerNode, aResultObserver);
    this._resultObserversList.push(aContainerNode);
  },

  unregisterForUpdates: function LM_unregisterForUpdates(aContainerNode)
  {
    this._resultObservers.delete(aContainerNode);
    let index = this._resultObserversList.indexOf(aContainerNode);
    this._resultObserversList.splice(index, 1);

    this._nodes.delete(aContainerNode);
  },

  _invalidateRegisteredContainers: function LM__invalidateRegisteredContainers()
  {
    for (let i = 0; i < this._resultObserversList.length; i++) {
      let container = this._resultObserversList[i];
      let observer = this._resultObservers.get(container);
      observer.invalidateContainer(container);
    }
  },

  updateURIVisitedStatus:
  function LM_updateURIVisitedStatus(aURI, aVisitedStatus)
  {
    for (let i = 0; i < this.children.length; i++) {
      if (this.children[i].uri.equals(aURI)) {
        this.children[i].visited = aVisitedStatus;
      }
    }

    for (let i = 0; i < this._resultObserversList.length; i++) {
      let container = this._resultObserversList[i];
      let observer = this._resultObservers.get(container);
      if (this._nodes.has(container)) {
        let nodes = this._nodes.get(container);
        for (let j = 0; j < nodes.length; j++) {
          let node = nodes[j];
          if (node.uri == aURI.spec) {
            Services.tm.mainThread.dispatch((function () {
              observer.nodeHistoryDetailsChanged(node, 0, aVisitedStatus);
            }).bind(this), Ci.nsIThread.DISPATCH_NORMAL);
          }
        }
      }
    }
  },

  /**
   * Terminates the livemark entry, cancelling any ongoing load.
   * Must be invoked before destroying the entry.
   */
  terminate: function LM_terminate()
  {
    // Avoid handling any updateChildren request from now on.
    this._terminated = true;
    // Clear the list before aborting, since abort() would try to set the
    // status and notify about it, but that's not really useful at this point.
    this._resultObserversList = [];
    this.abort();
  },

  /**
   * Aborts the livemark loading if needed.
   */
  abort: function LM_abort()
  {
    this.status = Ci.mozILivemark.STATUS_FAILED;
    if (this.loadGroup) {
      this.loadGroup.cancel(Cr.NS_BINDING_ABORTED);
      this.loadGroup = null;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.mozILivemark
  ])
}

////////////////////////////////////////////////////////////////////////////////
//// LivemarkLoadListener

/**
 * Object used internally to handle loading a livemark's contents.
 *
 * @param aLivemark
 *        The Livemark that is loading.
 */
function LivemarkLoadListener(aLivemark)
{
  this._livemark = aLivemark;
  this._processor = null;
  this._isAborted = false;
  this._ttl = EXPIRE_TIME_MS;
}

LivemarkLoadListener.prototype = {
  abort: function LLL_abort(aException)
  {
    if (!this._isAborted) {
      this._isAborted = true;
      this._livemark.abort();
      this._setResourceTTL(ONERROR_EXPIRE_TIME_MS);
    }
  },

  // nsIFeedResultListener
  handleResult: function LLL_handleResult(aResult)
  {
    if (this._isAborted) {
      return;
    }

    try {
      // We need this to make sure the item links are safe
      let feedPrincipal = secMan.getCodebasePrincipal(this._livemark.feedURI);

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
          secMan.checkLoadURIWithPrincipal(feedPrincipal, uri, SEC_FLAGS);
        }
        catch(ex) {
          continue;
        }

        let title = entry.title ? entry.title.plainText() : "";
        livemarkChildren.push({ uri: uri, title: title, visited: false });
      }

      this._livemark.children = livemarkChildren;
    }
    catch (ex) {
      this.abort(ex);
    }
    finally {
      this._processor.listener = null;
      this._processor = null;
    }
  },

  onDataAvailable: function LLL_onDataAvailable(aRequest, aContext,
                                                aInputStream, aSourceOffset,
                                                aCount)
  {
    if (this._processor) {
      this._processor.onDataAvailable(aRequest, aContext, aInputStream,
                                      aSourceOffset, aCount);
    }
  },

  onStartRequest: function LLL_onStartRequest(aRequest, aContext)
  {
    if (this._isAborted) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    try {
      // Parse feed data as it comes in
      this._processor = Cc["@mozilla.org/feed-processor;1"].
                        createInstance(Ci.nsIFeedProcessor);
      this._processor.listener = this;
      this._processor.parseAsync(null, channel.URI);
      this._processor.onStartRequest(aRequest, aContext);
    }
    catch (ex) {
      Components.utils.reportError("Livemark Service: feed processor received an invalid channel for " + channel.URI.spec);
      this.abort(ex);
    }
  },

  onStopRequest: function LLL_onStopRequest(aRequest, aContext, aStatus)
  {
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
        let entryInfo = channel.cacheToken.QueryInterface(Ci.nsICacheEntryInfo);
        if (entryInfo) {
          // nsICacheEntryInfo returns value as seconds.
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
    }
    catch (ex) {
      this.abort(ex);
    }
    finally {
      if (this._livemark.status == Ci.mozILivemark.STATUS_LOADING) {
        this._livemark.status = Ci.mozILivemark.STATUS_READY;
      }
      this._livemark.locked = false;
      this._livemark.loadGroup = null;
    }
  },

  _setResourceTTL: function LLL__setResourceTTL(aMilliseconds)
  {
    this._livemark.expireTime = Date.now() + aMilliseconds;
  },

  // nsIBadCertListener2
  notifyCertProblem: function LLL_certProblem(aSocketInfo, aStatus, aTargetSite)
  {
    return true;
  },

  // nsISSLErrorListener
  notifySSLError: function LLL_SSLError(aSocketInfo, aError, aTargetSite)
  {
    return true;
  },

  // nsIInterfaceRequestor
  getInterface: function LLL_getInterface(aIID)
  {
    return this.QueryInterface(aIID);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFeedResultListener
  , Ci.nsIStreamListener
  , Ci.nsIRequestObserver
  , Ci.nsIBadCertListener2
  , Ci.nsISSLErrorListener
  , Ci.nsIInterfaceRequestor
  ])
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([LivemarkService]);
