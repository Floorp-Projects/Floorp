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
 * The Original Code is the Places JS Livemark Service.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com> (C++ author)
 *   Joe Hughes <joe@retrovirus.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Robert Sayre <sayrer@gmail.com> (JS port)
 *   Phil Ringnalda <philringnalda@gmail.com>
 *   Marco Bonardo <mak77@bonardo.net>
 *   Takeshi Ichimaru <ayakawa.m@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of either the GNU General Public License Version 2 or later
 * (the "GPL"), or the GNU Lesser General Public License Version 2.1
 * or later (the "LGPL"), in which case the provisions of the GPL or
 * the LGPL are applicable instead of those above. If you wish to
 * allow use of your version of this file only under the terms of
 * either the GPL or the LGPL, and not to allow others to use your
 * version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the
 * notice and other provisions required by the GPL or the LGPL. If you
 * do not delete the provisions above, a recipient may use your
 * version of this file under the terms of any one of the MPL, the GPL
 * or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/* Update Behavior:
 *
 * The update timer is started via nsILivemarkService.start(). it fires
 * immediately and by default every 15 minutes (is dynamically calculated as
 * gExpiration/4), with a maximum limit of 1 hour.
 *
 * When the update timer fires, it iterates over the list of livemarks, and will
 * refresh a livemark *only* if it's expired.  The update is done in chunks and
 * each chunk is separated by a delay.
 *
 * The expiration time for a livemark is determined by using information
 * provided by the server when the feed was requested, specifically
 * nsICacheEntryInfo.expirationTime. If no information was provided by the 
 * server, the default expiration time is 1 hour.
 *
 * Users can modify the default expiration time via the following preferences:
 * browser.bookmarks.livemark_refresh_seconds: seconds between updates.
 * browser.bookmarks.livemark_refresh_limit_count: number of livemarks in a chunk.
 * browser.bookmarks.livemark_refresh_delay_time: seconds between each chunk.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "idle",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

XPCOMUtils.defineLazyServiceGetter(this, "secMan",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");

const SEC_FLAGS = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;

const PREF_REFRESH_SECONDS = "browser.bookmarks.livemark_refresh_seconds";
const PREF_REFRESH_LIMIT_COUNT = "browser.bookmarks.livemark_refresh_limit_count";
const PREF_REFRESH_DELAY_TIME = "browser.bookmarks.livemark_refresh_delay_time";

// Expire livemarks after 1 hour by default (milliseconds).
let gExpiration = 3600000;

// Number of livemarks that are read at once.
let gLimitCount = 1;

// Interval in seconds between refreshes of each group of livemarks.
let gDelayTime  = 3;

// Expire livemarks after this time on error (milliseconds).
const ERROR_EXPIRATION = 600000;

// Time after which we will stop checking for livemarks updates (milliseconds).
const IDLE_TIMELIMIT = 1800000;

// Maximum time between update checks (milliseconds).
// This cap is used only if the user sets a very high expiration time (>4h)
const MAX_REFRESH_TIME = 3600000;
// Minimum time between update checks, used to avoid flooding servers.
const MIN_REFRESH_TIME = 600000;

// Tracks the loading status 
const STATUS = {
  IDLE: 0,
  LOADING: 1,
  FAILED: 2,
}

function LivemarkService() {
  this._loadPrefs();

  // Cache of Livemark objects, hashed by folderId.
  XPCOMUtils.defineLazyGetter(this, "_livemarks", function () {
    let livemarks = {};
    PlacesUtils.annotations
               .getItemsWithAnnotation(PlacesUtils.LMANNO_FEEDURI)
               .forEach(function (aFolderId) {
                  livemarks[aFolderId] = new Livemark(aFolderId);
                });
    return livemarks;
  });

  // Cleanup on shutdown.
  Services.obs.addObserver(this, PlacesUtils.TOPIC_SHUTDOWN, false);

  // Observe bookmarks changes.
  PlacesUtils.addLazyBookmarkObserver(this);
}

LivemarkService.prototype = {
  _updateTimer: null,
  start: function LS_start() {
    if (this._updateTimer) {
      return;
    }

    // start is called in delayed startup, 5s after browser startup
    // we do a first check of the livemarks here, next checks will be on timer
    // browser start => 5s => this.start() => check => refresh_time => check
    this._checkAllLivemarks();
  },

  stopUpdateLivemarks: function LS_stopUpdateLivemarks() {
    for each (let livemark in this._livemarks) {
      livemark.abort();
    }
    // Stop the update timer.
    if (this._updateTimer) {
      this._updateTimer.cancel();
      this._updateTimer = null;
    }
  },

  _loadPrefs: function LS__loadPrefs() {
    try {
      let livemarkRefresh = Services.prefs.getIntPref(PREF_REFRESH_SECONDS);
      // Don't allow setting a too small timeout.
      gExpiration = Math.max(livemarkRefresh * 1000, MIN_REFRESH_TIME);
    }
    catch (ex) { /* no pref, use default */ }

    try {
      let limitCount = Services.prefs.getIntPref(PREF_REFRESH_LIMIT_COUNT);
      // Don't allow 0 or negative values.
      gLimitCount = Math.max(limitCount, gLimitCount);
    }
    catch (ex) { /* no pref, use default */ }

    try {
      let delayTime = Services.prefs.getIntPref(PREF_REFRESH_DELAY_TIME);
      // Don't allow too small delays.
      gDelayTime = Math.max(delayTime, gDelayTime);
    }
    catch (ex) { /* no pref, use default */ }
  },

  // nsIObserver
  observe: function LS_observe(aSubject, aTopic, aData) {
    if (aTopic == PlacesUtils.TOPIC_SHUTDOWN) {
      Services.obs.removeObserver(this, aTopic);
      // Remove bookmarks observer.
      PlacesUtils.removeLazyBookmarkObserver(this);
      // Stop updating livemarks.
      this.stopUpdateLivemarks();
    }
  },

  // We try to distribute the load of the livemark update.
  // load gLimitCount Livemarks per gDelayTime sec.
  _updatedLivemarks: [],
  _forceUpdate: false,
  _checkAllLivemarks: function LS__checkAllLivemarks(aForceUpdate) {
    if (aForceUpdate && !this._forceUpdate)
      this._forceUpdate = true;
    let updateCount = 0;
    for each (let livemark in this._livemarks) {
      if (this._updatedLivemarks.indexOf(livemark.folderId) == -1) {
        this._updatedLivemarks.push(livemark.folderId);
        // Check if livemarks are expired, update if needed.
        if (livemark.updateChildren(this._forceUpdate)) {
          if (++updateCount == gLimitCount) {
            break;
          }
        }
      }
    }

    let refresh_time = gDelayTime * 1000;
    if (this._updatedLivemarks.length >= Object.keys(this._livemarks).length) {
      // All livemarks are up-to-date, sleep until next period.
      this._updatedLivemarks.length = 0;
      this._forceUpdate = false;
      refresh_time = Math.min(Math.floor(gExpiration / 4), MAX_REFRESH_TIME);
    }
    this._newTimer(refresh_time);
  },

  _newTimer: function LS__newTimer(aTime) {
    if (this._updateTimer) {
      this._updateTimer.cancel();
    }
    this._updateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._updateTimer.initWithCallback(this._checkAllLivemarks.bind(this),
                                       aTime, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  createLivemark: function LS_createLivemark(aParentId, aName, aSiteURI,
                                             aFeedURI, aIndex) {
    let folderId = this.createLivemarkFolderOnly(aParentId, aName, aSiteURI,
                                                 aFeedURI, aIndex);
    this._livemarks[folderId].updateChildren();
    return folderId;
  },

  createLivemarkFolderOnly:
  function LS_createLivemarkFolderOnly(aParentId, aName, aSiteURI,
                                       aFeedURI, aIndex) {
    if (!aParentId || aParentId < 1 || !aFeedURI ||
        this.isLivemark(aParentId)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let livemark = new Livemark({ parentId: aParentId,
                                  index: aIndex,
                                  title: aName });

    // Add to the cache before setting the feedURI, otherwise isLivemark may
    // fail identifying it, when invoked by an annotation observer.
    this._livemarks[livemark.folderId] = livemark;

    livemark.feedURI = aFeedURI;
    if (aSiteURI) {
      livemark.siteURI = aSiteURI;
    }

    return livemark.folderId;
  },

  isLivemark: function LS_isLivemark(aFolderId) {
    if (aFolderId < 1) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    return aFolderId in this._livemarks;
  },

  getLivemarkIdForFeedURI: function LS_getLivemarkIdForFeedURI(aFeedURI) {
    if (!(aFeedURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    for each (livemark in this._livemarks) {
      if (livemark.feedURI.equals(aFeedURI)) {
        return livemark.folderId;
      }
    }

    return -1;
  },

  _ensureLivemark: function LS__ensureLivemark(aFolderId) {
    if (!this.isLivemark(aFolderId)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
  },

  getSiteURI: function LS_getSiteURI(aFolderId) {
    this._ensureLivemark(aFolderId);

    return this._livemarks[aFolderId].siteURI;
  },

  setSiteURI: function LS_setSiteURI(aFolderId, aSiteURI) {
    if (aSiteURI && !(aSiteURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._ensureLivemark(aFolderId);

    this._livemarks[aFolderId].siteURI = aSiteURI;
  },

  getFeedURI: function LS_getFeedURI(aFolderId) {
    this._ensureLivemark(aFolderId);

    return this._livemarks[aFolderId].feedURI;
  },

  setFeedURI: function LS_setFeedURI(aFolderId, aFeedURI) {
    if (!aFeedURI || !(aFeedURI instanceof Ci.nsIURI)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._ensureLivemark(aFolderId);

    this._livemarks[aFolderId].feedURI = aFeedURI;
  },

  reloadAllLivemarks: function LS_reloadAllLivemarks() {
    this._checkAllLivemarks(true);
  },

  reloadLivemarkFolder: function LS_reloadLivemarkFolder(aFolderId) {
    this._ensureLivemark(aFolderId);

    this._livemarks[aFolderId].updateChildren(true);
  },

  // nsINavBookmarkObserver
  onBeginUpdateBatch: function() { },
  onEndUpdateBatch: function() { },
  onItemAdded: function() { },
  onItemChanged: function() { },
  onItemVisited: function() { },
  onItemMoved: function() { },
  onBeforeItemRemoved: function() { },

  onItemRemoved: function(aItemId, aParentId, aIndex, aItemType) {
    // we don't need to remove annotations since itemAnnotations
    // are already removed with the bookmark
    if (!this.isLivemark(aItemId)) {
      return;
    }

    this._livemarks[aItemId].terminate();
    delete this._livemarks[aItemId];
  },

  // nsISupports
  classID: Components.ID("{dca61eb5-c7cd-4df1-b0fb-d0722baba251}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsILivemarkService
  , Ci.nsINavBookmarkObserver
  , Ci.nsIObserver
  ])
};

/**
 * Object used internally to represent a livemark.
 *
 * @param aFolderIdOrCreationInfo
 *        Item id of the bookmarks folder representing an existing livemark, or
 *        object containing information (parentId, title, index) to create a
 *        new livemark.
 *
 * @note terminate() must be invoked before getting rid of this object.
 */
function Livemark(aFolderIdOrCreationInfo)
{
  if (typeof(aFolderIdOrCreationInfo) == "number") {
    this.folderId = aFolderIdOrCreationInfo;
  }
  else if (typeof(aFolderIdOrCreationInfo) == "object"){
    this.folderId =
      PlacesUtils.bookmarks.createFolder(aFolderIdOrCreationInfo.parentId,
                                         aFolderIdOrCreationInfo.title,
                                         aFolderIdOrCreationInfo.index);
    PlacesUtils.bookmarks.setFolderReadonly(this.folderId, true);

    // Setup the status to avoid some useless lazy getter work.
    this._loadStatus = STATUS.IDLE;
  }
  else {
    throw Cr.NS_ERROR_UNEXPECTED;
  }
}

Livemark.prototype = {
  loadGroup: null,
  locked: null,

  /**
   * Whether this Livemark is valid or has been terminate()d.
   */
  get alive() !!this.folderId,

  /**
   * Sets an item annotation on the folderId representing this livemark.
   *
   * @param aName
   *        Name of the annotation.
   * @param aValue
   *        Value of the annotation.
   * @return The annotation value.
   * @throws If the folder is invalid.
   */
  _setAnno: function LM__setAnno(aName, aValue)
  {
    if (this.alive) {
      PlacesUtils.annotations
                 .setItemAnnotation(this.folderId, aName, aValue, 0,
                                    PlacesUtils.annotations.EXPIRE_NEVER);
    }
    return aValue;
  },

  /**
   * Gets a item annotation from the folderId representing this livemark.
   *
   * @param aName
   *        Name of the annotation.
   * @return The annotation value.
   * @throws If the folder is invalid or the annotation does not exist.
   */
  _getAnno: function LM__getAnno(aName)
  {
    if (this.alive) {
      return PlacesUtils.annotations.getItemAnnotation(this.folderId, aName);
    }
    return null;
  },

  /**
   * Removes a item annotation from the folderId representing this livemark.
   *
   * @param aName
   *        Name of the annotation.
   * @throws If the folder is invalid or the annotation does not exist.
   */
  _removeAnno: function LM__removeAnno(aName)
  {
    if (this.alive) {
      return PlacesUtils.annotations.removeItemAnnotation(this.folderId, aName);
    }
  },

  set feedURI(aFeedURI)
  {
    this._setAnno(PlacesUtils.LMANNO_FEEDURI, aFeedURI.spec);
    this._feedURI = aFeedURI;
  },
  get feedURI()
  {
    if (this._feedURI === undefined) {
      this._feedURI = NetUtil.newURI(this._getAnno(PlacesUtils.LMANNO_FEEDURI));
    }
    return this._feedURI;
  },

  set siteURI(aSiteURI)
  {
    if (!aSiteURI) {
      this._removeAnno(PlacesUtils.LMANNO_SITEURI);
      this._siteURI = null;
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
    this._siteURI = aSiteURI;
  },
  get siteURI()
  {
    if (this._siteURI === undefined) {
      try {
        this._siteURI = NetUtil.newURI(this._getAnno(PlacesUtils.LMANNO_SITEURI));
      } catch (ex if ex.result == Cr.NS_ERROR_NOT_AVAILABLE) {
        // siteURI is optional.
        return this._siteURI = null;
      }
    }
    return this._siteURI;
  },

  set expireTime(aExpireTime)
  {
    this._expireTime = this._setAnno(PlacesUtils.LMANNO_EXPIRATION,
                                     aExpireTime);
  },
  get expireTime()
  {
    if (this._expireTime === undefined) {
      try {
        this._expireTime = this._getAnno(PlacesUtils.LMANNO_EXPIRATION);
      } catch (ex if ex.result == Cr.NS_ERROR_NOT_AVAILABLE) {
        // Expiration time may not be set yet.
        return this._expireTime = 0;
      }
    }
    return this._expireTime;
  },

  set loadStatus(aStatus)
  {
    // Avoid setting the same status multiple times.
    if (this.loadStatus == aStatus) {
      return;
    }

    switch (aStatus) {
      case STATUS.FAILED:
        if (this.loadStatus == STATUS.LOADING) {
          this._removeAnno(PlacesUtils.LMANNO_LOADING);
        }
        this._setAnno(PlacesUtils.LMANNO_LOADFAILED, true);
        break;
      case STATUS.LOADING:
        if (this.loadStatus == STATUS.FAILED) {
          this._removeAnno(PlacesUtils.LMANNO_LOADFAILED);
        }
        this._setAnno(PlacesUtils.LMANNO_LOADING, true);
        break;
      default:
        if (this.loadStatus == STATUS.LOADING) {
          this._removeAnno(PlacesUtils.LMANNO_LOADING);
        }
        else if (this.loadStatus == STATUS.FAILED) {
          this._removeAnno(PlacesUtils.LMANNO_LOADFAILED);
        }
        break;
    }

    this._loadStatus = aStatus;
  },
  get loadStatus()
  {
    if (this._loadStatus === undefined) {
      if (PlacesUtils.annotations.itemHasAnnotation(this.folderId,
                                                    PlacesUtils.LMANNO_LOADFAILED)) {
        this._loadStatus = STATUS.FAILED;
      }
      else if (PlacesUtils.annotations.itemHasAnnotation(this.folderId,
                                                         PlacesUtils.LMANNO_LOADING)) {
        this._loadStatus = STATUS.LOADING;
      }
      else {
        this._loadStatus = STATUS.IDLE;
      }
    }
    return this._loadStatus;
  },

  /**
   * Tries to updates the livemark if needed.
   * The update process is asynchronous.
   *
   * @param [optional] aForceUpdate
   *        If true will try to update the livemark even if its contents have
   *        not yet expired.
   * @return true if the livemarks will be updated, false otherwise.
   */
  updateChildren: function LM_updateChildren(aForceUpdate)
  {
    if (this.locked) {
      return false;
    }

    this.locked = true;

    // Check the TTL/expiration on this.  If there isn't one,
    // then we assume it's never been loaded.  We perform this
    // check even when the update is being forced, in case the
    // livemark has somehow never been loaded.
    if (!aForceUpdate && this.expireTime > Date.now()) {
      // No need to refresh this livemark.
      this.locked = false;
      return false;
    }

    // Check the user idle time.
    // If the user is away from the computer, don't bother updating,
    // so we save some bandwidth. 
    // If we can't get the idle time, assume the user is not idle.
    try {
      let idleTime = idle.idleTime;
      if (idleTime > IDLE_TIMELIMIT && !aForceUpdate) {
        this.locked = false;
        return false;
      }
    }
    catch (ex) {}

    let loadgroup;
    try {
      // Create a load group for the request.  This will allow us to
      // automatically keep track of redirects, so we can always
      // cancel the channel.
      loadgroup = Cc["@mozilla.org/network/load-group;1"].
                  createInstance(Ci.nsILoadGroup);
      let channel = NetUtil.newChannel(this.feedURI.spec).
                    QueryInterface(Ci.nsIHttpChannel);
      channel.loadGroup = loadgroup;
      channel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND |
                           Ci.nsIRequest.VALIDATE_ALWAYS;
      channel.requestMethod = "GET";
      channel.setRequestHeader("X-Moz", "livebookmarks", false);

      // Stream the result to the feed parser with this listener
      let listener = new LivemarkLoadListener(this);
      channel.notificationCallbacks = listener;

      this.loadStatus = STATUS.LOADING;
      channel.asyncOpen(listener, null);
    }
    catch (ex) {
      this.loadStatus = STATUS.FAILED;
      this.locked = false;
      return false;
    }
    this.loadGroup = loadgroup;
    return true;
  },

  /**
   * Replaces all children of the livemark.
   *
   * @param aChildren
   *        Array of new children in the form of { uri, title } objects.
   */
  replaceChildren: function LM_replaceChildren(aChildren) {
    let self = this;
    PlacesUtils.bookmarks.runInBatchMode({
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsINavHistoryBatchCallback
      ]),
      runBatched: function LM_runBatched(aUserData) {
        PlacesUtils.bookmarks.removeFolderChildren(self.folderId);
        aChildren.forEach(function (aChild) {
          PlacesUtils.bookmarks.insertBookmark(self.folderId,
                                               aChild.uri,
                                               PlacesUtils.bookmarks.DEFAULT_INDEX,
                                               aChild.title);
        });
      }
    }, null);
  },

  /**
   * Terminates the livemark entry, cancelling any ongoing load.
   * Must be invoked before destroying the entry.
   */
  terminate: function LM_terminate()
  {
    this.folderId = null;
    this.abort();
  },

  /**
   * Aborts the livemark loading if needed.
   */
  abort: function LM_abort() {
    this.locked = false;
    if (this.loadGroup) {
      this.loadStatus = STATUS.FAILED;
      this.loadGroup.cancel(Cr.NS_BINDING_ABORTED);
      this.loadGroup = null;
    }
  },
}

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
  this._ttl = gExpiration;
}

LivemarkLoadListener.prototype = {
  abort: function LLL_abort() {
    this._isAborted = true;
    this._livemark.abort();
  },

  // nsIFeedResultListener
  handleResult: function LLL_handleResult(aResult) {
    if (this._isAborted) {
      return;
    }

    try {
      // We need this to make sure the item links are safe
      let feedPrincipal = secMan.getCodebasePrincipal(this._livemark.feedURI);

      // Enforce well-formedness because the existing code does
      if (!aResult || !aResult.doc || aResult.bozo) {
        this.abort();
        this._ttl = gExpiration;
        throw Cr.NS_ERROR_FAILURE;
      }

      let feed = aResult.doc.QueryInterface(Ci.nsIFeed);
      let siteURI = this._livemark.siteURI;
      if (feed.link && (!siteURI || !feed.link.equals(siteURI))) {
        this._livemark.siteURI = siteURI = feed.link;
      }

      // Insert feed items.
      let livemarkChildren = [];
      for (let i = 0; i < feed.items.length; ++i) {
        let entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
        let href = entry.link || siteURI;
        if (!href) {
          continue;
        }

        try {
          secMan.checkLoadURIWithPrincipal(feedPrincipal, href, SEC_FLAGS);
        }
        catch(ex) {
          continue;
        }

        let title = entry.title ? entry.title.plainText() : "";
        livemarkChildren.push({ uri: href, title: title });
      }

      this._livemark.replaceChildren(livemarkChildren);
    }
    catch (ex) {
      this.abort();
    }
    finally {
      this._processor.listener = null;
      this._processor = null;
    }
  },

  onDataAvailable: function LLL_onDataAvailable(aRequest, aContext, aInputStream,
                                                aSourceOffset, aCount) {
    if (this._processor) {
      this._processor.onDataAvailable(aRequest, aContext, aInputStream,
                                      aSourceOffset, aCount);
    }
  },

  onStartRequest: function LLL_onStartRequest(aRequest, aContext) {
    if (this._isAborted) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._livemark.loadStatus = STATUS.LOADING;

    let channel = aRequest.QueryInterface(Ci.nsIChannel);

    // Parse feed data as it comes in
    this._processor = Cc["@mozilla.org/feed-processor;1"].
                      createInstance(Ci.nsIFeedProcessor);
    this._processor.listener = this;
    this._processor.parseAsync(null, channel.URI);

    try {
      this._processor.onStartRequest(aRequest, aContext);
    }
    catch (ex) {
      Components.utils.reportError("Livemark Service: feed processor received an invalid channel for " + channel.URI.spec);
    }
  },

  onStopRequest: function LLL_onStopRequest(aRequest, aContext, aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      this.abort();
      this._setResourceTTL(ERROR_EXPIRATION);
      return;
    }

    if (this._livemark.loadStatus == STATUS.LOADING) {
      this._livemark.loadStatus = STATUS.IDLE;
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
          // nsICacheEntryInfo returns value as seconds,
          // expireTime stores as milliseconds
          let expireTime = entryInfo.expirationTime * 1000;
          let nowTime = Date.now();

          // note, expireTime can be 0, see bug 383538
          if (expireTime > nowTime) {
            this._setResourceTTL(Math.max((expireTime - nowTime), gExpiration));
            return;
          }
        }
      }
    }
    catch (ex) {
      this.abort();
    }
    finally {
      this._livemark.locked = false;
      this._livemark.loadGroup = null;
    }
    this._setResourceTTL(this._ttl);
  },

  _setResourceTTL: function LLL__setResourceTTL(aMilliseconds) {
    this._livemark.expireTime = Date.now() + aMilliseconds;
  },

  // nsIBadCertListener2
  notifyCertProblem:
  function LLL_certProblem(aSocketInfo, aStatus, aTargetSite) {
    return true;
  },

  // nsISSLErrorListener
  notifySSLError: function LLL_SSLError(aSocketInfo, aError, aTargetSite) {
    return true;
  },

  // nsIInterfaceRequestor
  getInterface: function LLL_getInterface(aIID) {
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
