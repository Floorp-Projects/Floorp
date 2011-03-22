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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// Global service getters.
XPCOMUtils.defineLazyServiceGetter(this, "bms",
                                   "@mozilla.org/browser/nav-bookmarks-service;1",
                                   "nsINavBookmarksService");
XPCOMUtils.defineLazyServiceGetter(this, "ans",
                                   "@mozilla.org/browser/annotation-service;1",
                                   "nsIAnnotationService");

const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";
const LMANNO_EXPIRATION = "livemark/expiration";
const LMANNO_LOADFAILED = "livemark/loadfailed";
const LMANNO_LOADING = "livemark/loading";

const PS_CONTRACTID = "@mozilla.org/preferences-service;1";
const NH_CONTRACTID = "@mozilla.org/browser/nav-history-service;1";
const OS_CONTRACTID = "@mozilla.org/observer-service;1";
const IO_CONTRACTID = "@mozilla.org/network/io-service;1";
const LG_CONTRACTID = "@mozilla.org/network/load-group;1";
const FP_CONTRACTID = "@mozilla.org/feed-processor;1";
const SEC_CONTRACTID = "@mozilla.org/scriptsecuritymanager;1";
const IS_CONTRACTID = "@mozilla.org/widget/idleservice;1";
const LS_CONTRACTID = "@mozilla.org/browser/livemark-service;2";

const SEC_FLAGS = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;

const PREF_REFRESH_SECONDS = "browser.bookmarks.livemark_refresh_seconds";
const PREF_REFRESH_LIMIT_COUNT = "browser.bookmarks.livemark_refresh_limit_count";
const PREF_REFRESH_DELAY_TIME = "browser.bookmarks.livemark_refresh_delay_time";

// Expire livemarks after 1 hour by default (milliseconds).
var gExpiration = 3600000;

// Number of livemarks that are read at once.
var gLimitCount = 1;

// Interval in seconds between refreshes of each group of livemarks.
var gDelayTime  = 3;

// Expire livemarks after this time on error (milliseconds).
const ERROR_EXPIRATION = 600000;

// Time after which we will stop checking for livemarks updates (milliseconds).
const IDLE_TIMELIMIT = 1800000;

// Maximum time between update checks (milliseconds).
// This cap is used only if the user sets a very high expiration time (>4h)
const MAX_REFRESH_TIME = 3600000;
// Minimum time between update checks, used to avoid flooding servers.
const MIN_REFRESH_TIME = 600000;

const TOPIC_SHUTDOWN = "places-shutdown";

function MarkLivemarkLoadFailed(aFolderId) {
  // Bail out if this failed before.
  if (ans.itemHasAnnotation(aFolderId, LMANNO_LOADFAILED))
    return;

  // removeItemAnnotation does not care whether the anno exists.
  ans.removeItemAnnotation(aFolderId, LMANNO_LOADING);
  ans.setItemAnnotation(aFolderId, LMANNO_LOADFAILED, true,
                        0, ans.EXPIRE_NEVER);
}

function LivemarkService() {
  // TODO: prefs should be under places.livemarks.xxx and we should observe that
  // branch for changes.
  this._prefs = Cc[PS_CONTRACTID].getService(Ci.nsIPrefBranch);
  this._loadPrefs();

  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  XPCOMUtils.defineLazyServiceGetter(this, "_idleService", IS_CONTRACTID,
                                     "nsIIdleService");

  // Load current livemarks.
  this._ios = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);
  var livemarks = ans.getItemsWithAnnotation(LMANNO_FEEDURI);
  for (let i = 0; i < livemarks.length; i++) {
    let spec = ans.getItemAnnotation(livemarks[i], LMANNO_FEEDURI);
    this._pushLivemark(livemarks[i], this._ios.newURI(spec, null, null));
  }

  // Cleanup on shutdown.
  this._obs = Cc[OS_CONTRACTID].getService(Ci.nsIObserverService);
  this._obs.addObserver(this, TOPIC_SHUTDOWN, false);

  // Observe bookmarks changes.
  bms.addObserver(this, false);
}

LivemarkService.prototype = {
  // [ {folderId:, folderURI:, feedURI:, loadGroup:, locked: } ];
  _livemarks: [],

  _updateTimer: null,
  start: function LS_start() {
    if (this._updateTimer)
      return;
    // start is called in delayed startup, 5s after browser startup
    // we do a first check of the livemarks here, next checks will be on timer
    // browser start => 5s => this.start() => check => refresh_time => check
    this._checkAllLivemarks();
  },

  stopUpdateLivemarks: function LS_stopUpdateLivemarks() {
    for (var livemark in this._livemarks) {
      if (livemark.loadGroup)
        livemark.loadGroup.cancel(Components.results.NS_BINDING_ABORTED);
    }
    // kill timer
    if (this._updateTimer) {
      this._updateTimer.cancel();
      this._updateTimer = null;
    }
  },

  _pushLivemark: function LS__pushLivemark(aFolderId, aFeedURI) {
    // returns new length of _livemarks
    return this._livemarks.push({folderId: aFolderId, feedURI: aFeedURI});
  },

  _getLivemarkIndex: function LS__getLivemarkIndex(aFolderId) {
    for (var i = 0; i < this._livemarks.length; ++i) {
      if (this._livemarks[i].folderId == aFolderId)
        return i;
    }
    throw Cr.NS_ERROR_INVALID_ARG;
  },

  _loadPrefs: function LS__loadPrefs() {
    try {
      let livemarkRefresh = this._prefs.getIntPref(PREF_REFRESH_SECONDS);
      // Don't allow setting a too small timeout.
      gExpiration = Math.max(livemarkRefresh * 1000, MIN_REFRESH_TIME);
    }
    catch (ex) { /* no pref, use default */ }

    try {
      let limitCount = this._prefs.getIntPref(PREF_REFRESH_LIMIT_COUNT);
      // Don't allow 0 or negative values.
      gLimitCount = Math.max(limitCount, gLimitCount);
    }
    catch (ex) { /* no pref, use default */ }

    try {
      let delayTime = this._prefs.getIntPref(PREF_REFRESH_DELAY_TIME);
      // Don't allow too small delays.
      gDelayTime = Math.max(delayTime, gDelayTime);
    }
    catch (ex) { /* no pref, use default */ }
  },

  // nsIObserver
  observe: function LS_observe(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_SHUTDOWN) {
      this._obs.removeObserver(this, TOPIC_SHUTDOWN);

      // Remove bookmarks observer.
      bms.removeObserver(this);
      // Stop updating livemarks.
      this.stopUpdateLivemarks();
    }
  },

  // We try to distribute the load of the livemark update.
  // load gLimitCount Livemarks per gDelayTime sec.
  _nextUpdateStartIndex : 0,
  _checkAllLivemarks: function LS__checkAllLivemarks() {
    var startNo = this._nextUpdateStartIndex;
    var count = 0;
    for (var i = startNo; (i < this._livemarks.length) && (count < gLimitCount); ++i ) {
      // check if livemarks are expired, update if needed
      try {
        if (this._updateLivemarkChildren(i, false)) count++;
      }
      catch (ex) { }
      this._nextUpdateStartIndex = i+1;
    }

    let refresh_time = gDelayTime * 1000;
    if (this._nextUpdateStartIndex >= this._livemarks.length) {
      // all livemarks are checked, sleeping until next period
      this._nextUpdateStartIndex = 0;
      refresh_time = Math.min(Math.floor(gExpiration / 4), MAX_REFRESH_TIME);
    }
    this._newTimer(refresh_time);
  },

  _newTimer: function LS__newTimer(aTime) {
    if (this._updateTimer)
      this._updateTimer.cancel();
    this._updateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let self = this;
    this._updateTimer.initWithCallback({
      notify: function LS_T_notify() {
        self._checkAllLivemarks();
      },
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsITimerCallback
      ]),
    }, aTime, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  deleteLivemarkChildren: function LS_deleteLivemarkChildren(aFolderId) {
    bms.removeFolderChildren(aFolderId);
  },

  _updateLivemarkChildren:
  function LS__updateLivemarkChildren(aIndex, aForceUpdate) {
    if (this._livemarks[aIndex].locked)
      return false;

    var livemark = this._livemarks[aIndex];
    livemark.locked = true;
    try {
      // Check the TTL/expiration on this.  If there isn't one,
      // then we assume it's never been loaded.  We perform this
      // check even when the update is being forced, in case the
      // livemark has somehow never been loaded.
      var expireTime = ans.getItemAnnotation(livemark.folderId,
                                             LMANNO_EXPIRATION);
      if (!aForceUpdate && expireTime > Date.now()) {
        // no need to refresh
        livemark.locked = false;
        return false;
      }

      // Check the user idle time.
      // If the user is away from the computer, don't bother updating,
      // so we save some bandwidth. 
      // If we can't get the idle time, assume the user is not idle.
      var idleTime = 0;
      try {
        idleTime = this._idleService.idleTime;
      }
      catch (ex) { /* We don't care */ }
      if (idleTime > IDLE_TIMELIMIT) {
        livemark.locked = false;
        return false;
      }
    }
    catch (ex) {
      // This livemark has never been loaded, since it has no expire time.
    }

    var loadgroup;
    try {
      // Create a load group for the request.  This will allow us to
      // automatically keep track of redirects, so we can always
      // cancel the channel.
      loadgroup = Cc[LG_CONTRACTID].createInstance(Ci.nsILoadGroup);
      var uriChannel = this._ios.newChannel(livemark.feedURI.spec, null, null);
      uriChannel.loadGroup = loadgroup;
      uriChannel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND |
                              Ci.nsIRequest.VALIDATE_ALWAYS;
      var httpChannel = uriChannel.QueryInterface(Ci.nsIHttpChannel);
      httpChannel.requestMethod = "GET";
      httpChannel.setRequestHeader("X-Moz", "livebookmarks", false);

      // Stream the result to the feed parser with this listener
      var listener = new LivemarkLoadListener(livemark);
      // removeItemAnnotation can safely be used even when the anno isn't set
      ans.removeItemAnnotation(livemark.folderId, LMANNO_LOADFAILED);
      ans.setItemAnnotation(livemark.folderId, LMANNO_LOADING, true,
                            0, ans.EXPIRE_NEVER);
      httpChannel.notificationCallbacks = listener;
      httpChannel.asyncOpen(listener, null);
    }
    catch (ex) {
      MarkLivemarkLoadFailed(livemark.folderId);
      livemark.locked = false;
      return false;
    }
    livemark.loadGroup = loadgroup;
    return true;
  },

  createLivemark: function LS_createLivemark(aParentId, aName, aSiteURI,
                                             aFeedURI, aIndex) {
    if (!aParentId || !aFeedURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    // Don't add livemarks to livemarks
    if (this.isLivemark(aParentId))
      throw Cr.NS_ERROR_INVALID_ARG;

    var folderId = this._createFolder(aParentId, aName, aSiteURI,
                                      aFeedURI, aIndex);

    // do a first update of the livemark children
    this._updateLivemarkChildren(this._pushLivemark(folderId, aFeedURI) - 1,
                                 false);

    return folderId;
  },

  createLivemarkFolderOnly:
  function LS_createLivemarkFolderOnly(aParentId, aName, aSiteURI,
                                       aFeedURI, aIndex) {
    if (aParentId < 1 || !aFeedURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    // Don't add livemarks to livemarks
    if (this.isLivemark(aParentId))
      throw Cr.NS_ERROR_INVALID_ARG;

    var folderId = this._createFolder(aParentId, aName, aSiteURI,
                                      aFeedURI, aIndex);

    var livemarkIndex = this._pushLivemark(folderId, aFeedURI) - 1;
    var livemark = this._livemarks[livemarkIndex];
    return folderId;
  },

  _createFolder:
  function LS__createFolder(aParentId, aName, aSiteURI, aFeedURI, aIndex) {
    var folderId = bms.createFolder(aParentId, aName, aIndex);
    bms.setFolderReadonly(folderId, true);

    // Annotate this folder as being the last created livemark.  This is needed
    // by isLivemark since it is not aware of this livemark till _pushLivemark
    // is called, later during the addition path.
    this._lastCreatedLivemarkFolderId = folderId;
    // Add an annotation to map the folder id to the livemark feed URI
    ans.setItemAnnotation(folderId, LMANNO_FEEDURI, aFeedURI.spec,
                          0, ans.EXPIRE_NEVER);

    if (aSiteURI) {
      // Add an annotation to map the folder URI to the livemark site URI
      this._setSiteURISecure(folderId, aFeedURI, aSiteURI);
    }

    return folderId;
  },

  isLivemark: function LS_isLivemark(aFolderId) {
    if (aFolderId < 1)
      throw Cr.NS_ERROR_INVALID_ARG;
    try {
      this._getLivemarkIndex(aFolderId);
      return true;
    }
    catch (ex) {}
    // There is an edge case here, if a AnnotationChanged notification asks for
    // isLivemark and the livemark is currently being added, it is not yet in
    // the _livemarks array.  In such a case go the slow path.
    if (this._lastCreatedLivemarkFolderId === aFolderId)
      return ans.itemHasAnnotation(aFolderId, LMANNO_FEEDURI);
    return false;
  },

  getLivemarkIdForFeedURI: function LS_getLivemarkIdForFeedURI(aFeedURI) {
    if (!(aFeedURI instanceof Ci.nsIURI))
      throw Cr.NS_ERROR_INVALID_ARG;

    for (var i = 0; i < this._livemarks.length; ++i) {
      if (this._livemarks[i].feedURI.equals(aFeedURI))
        return this._livemarks[i].folderId;
    }

    return -1;
  },

  _ensureLivemark: function LS__ensureLivemark(aFolderId) {
    if (!this.isLivemark(aFolderId))
      throw Cr.NS_ERROR_INVALID_ARG;
  },

  getSiteURI: function LS_getSiteURI(aFolderId) {
    this._ensureLivemark(aFolderId);

    if (ans.itemHasAnnotation(aFolderId, LMANNO_SITEURI)) {
      var siteURIString = ans.getItemAnnotation(aFolderId, LMANNO_SITEURI);

      return this._ios.newURI(siteURIString, null, null);
    }
    return null;
  },

  setSiteURI: function LS_setSiteURI(aFolderId, aSiteURI) {
    this._ensureLivemark(aFolderId);

    if (!aSiteURI) {
      ans.removeItemAnnotation(aFolderId, LMANNO_SITEURI);
      return;
    }

    var livemarkIndex = this._getLivemarkIndex(aFolderId);
    var livemark = this._livemarks[livemarkIndex];
    this._setSiteURISecure(aFolderId, livemark.feedURI, aSiteURI);
  },

  _setSiteURISecure:
  function LS__setSiteURISecure(aFolderId, aFeedURI, aSiteURI) {
    var secMan = Cc[SEC_CONTRACTID].getService(Ci.nsIScriptSecurityManager);
    var feedPrincipal = secMan.getCodebasePrincipal(aFeedURI);
    try {
      secMan.checkLoadURIWithPrincipal(feedPrincipal, aSiteURI, SEC_FLAGS);
    }
    catch (e) {
      return;
    }
    ans.setItemAnnotation(aFolderId, LMANNO_SITEURI, aSiteURI.spec,
                          0, ans.EXPIRE_NEVER);
  },

  getFeedURI: function LS_getFeedURI(aFolderId) {
    if (ans.itemHasAnnotation(aFolderId, LMANNO_FEEDURI))
      return this._ios.newURI(ans.getItemAnnotation(aFolderId, LMANNO_FEEDURI),
                              null, null);
    return null;
  },

  setFeedURI: function LS_setFeedURI(aFolderId, aFeedURI) {
    if (!aFeedURI)
      throw Cr.NS_ERROR_INVALID_ARG;

    ans.setItemAnnotation(aFolderId, LMANNO_FEEDURI, aFeedURI.spec,
                          0, ans.EXPIRE_NEVER);

    // now update our internal table
    var livemarkIndex = this._getLivemarkIndex(aFolderId);
    this._livemarks[livemarkIndex].feedURI = aFeedURI;
  },

  reloadAllLivemarks: function LS_reloadAllLivemarks() {
    for (var i = 0; i < this._livemarks.length; ++i) {
      this._updateLivemarkChildren(i, true);
    }
  },

  reloadLivemarkFolder: function LS_reloadLivemarkFolder(aFolderId) {
    var livemarkIndex = this._getLivemarkIndex(aFolderId);
    this._updateLivemarkChildren(livemarkIndex, true);
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
    try {
      var livemarkIndex = this._getLivemarkIndex(aItemId);
    }
    catch(ex) {
      // not a livemark
      return;
    }
    var livemark = this._livemarks[livemarkIndex];

    // remove the livemark from the update array
    this._livemarks.splice(livemarkIndex, 1);

    if (livemark.loadGroup)
      livemark.loadGroup.cancel(Components.results.NS_BINDING_ABORTED);
  },

  // nsISupports
  classID: Components.ID("{dca61eb5-c7cd-4df1-b0fb-d0722baba251}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsILivemarkService
  , Ci.nsINavBookmarkObserver
  , Ci.nsIObserver
  ])
};

function LivemarkLoadListener(aLivemark) {
  this._livemark = aLivemark;
  this._processor = null;
  this._isAborted = false;
  this._ttl = gExpiration;
}

LivemarkLoadListener.prototype = {
  abort: function LLL_abort() {
    this._isAborted = true;
  },

  // called back from handleResult
  runBatched: function LLL_runBatched(aUserData) {
    var result = aUserData.QueryInterface(Ci.nsIFeedResult);

    // We need this to make sure the item links are safe
    var secMan = Cc[SEC_CONTRACTID].getService(Ci.nsIScriptSecurityManager);
    var feedPrincipal = secMan.getCodebasePrincipal(this._livemark.feedURI);

    var lmService = Cc[LS_CONTRACTID].getService(Ci.nsILivemarkService);

    // Enforce well-formedness because the existing code does
    if (!result || !result.doc || result.bozo) {
      MarkLivemarkLoadFailed(this._livemark.folderId);
      this._ttl = gExpiration;
      throw Cr.NS_ERROR_FAILURE;
    }

    // Clear out any child nodes of the livemark folder, since
    // they're about to be replaced.
    this.deleteLivemarkChildren(this._livemark.folderId);
    var feed = result.doc.QueryInterface(Ci.nsIFeed);
    if (feed.link) {
      var oldSiteURI = lmService.getSiteURI(this._livemark.folderId);
      if (!oldSiteURI || !feed.link.equals(oldSiteURI))
        lmService.setSiteURI(this._livemark.folderId, feed.link);
    }
    // Loop through and check for a link and a title
    // as the old code did
    for (var i = 0; i < feed.items.length; ++i) {
      let entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      let href = entry.link;
      if (!href)
        continue;

      let title = entry.title ? entry.title.plainText() : "";

      try {
        secMan.checkLoadURIWithPrincipal(feedPrincipal, href, SEC_FLAGS);
      }
      catch(ex) {
        continue;
      }

      this.insertLivemarkChild(this._livemark.folderId, href, title);
    }
  },

  /**
   * See nsIFeedResultListener.idl
   */
  handleResult: function LLL_handleResult(aResult) {
    if (this._isAborted) {
      MarkLivemarkLoadFailed(this._livemark.folderId);
      this._livemark.locked = false;
      return;
    }
    try {
      // The actual work is done in runBatched, see above.
      bms.runInBatchMode(this, aResult);
    }
    finally {
      this._processor.listener = null;
      this._processor = null;
      this._livemark.locked = false;
      ans.removeItemAnnotation(this._livemark.folderId, LMANNO_LOADING);
    }
  },

  deleteLivemarkChildren: LivemarkService.prototype.deleteLivemarkChildren,

  insertLivemarkChild:
  function LS_insertLivemarkChild(aFolderId, aUri, aTitle) {
    bms.insertBookmark(aFolderId, aUri, bms.DEFAULT_INDEX, aTitle);
  },

  /**
   * See nsIStreamListener.idl
   */
  onDataAvailable: function LLL_onDataAvailable(aRequest, aContext, aInputStream,
                                                aSourceOffset, aCount) {
    if (this._processor)
      this._processor.onDataAvailable(aRequest, aContext, aInputStream,
                                      aSourceOffset, aCount);
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest: function LLL_onStartRequest(aRequest, aContext) {
    if (this._isAborted)
      throw Cr.NS_ERROR_UNEXPECTED;

    var channel = aRequest.QueryInterface(Ci.nsIChannel);

    // Parse feed data as it comes in
    this._processor = Cc[FP_CONTRACTID].createInstance(Ci.nsIFeedProcessor);
    this._processor.listener = this;
    this._processor.parseAsync(null, channel.URI);

    try {
      this._processor.onStartRequest(aRequest, aContext);
    }
    catch (ex) {
      Components.utils.reportError("Livemark Service: feed processor received an invalid channel for " + channel.URI.spec);
    }
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest: function LLL_onStopRequest(aRequest, aContext, aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      this._isAborted = true;
      this._livemark.locked = false;
      var lmService = Cc[LS_CONTRACTID].getService(Ci.nsILivemarkService);
      // One of the reasons we could abort a request is when a livemark is
      // removed, in such a case the livemark itemId would already be invalid.
      if (lmService.isLivemark(this._livemark.folderId)) {
        // Something went wrong, try to load again in a bit
        this._setResourceTTL(ERROR_EXPIRATION);
        MarkLivemarkLoadFailed(this._livemark.folderId);
      }
      return;
    }
    // Set an expiration on the livemark, for reloading the data
    try {
      if (this._processor)
        this._processor.onStopRequest(aRequest, aContext, aStatus);

      // Calculate a new ttl
      var channel = aRequest.QueryInterface(Ci.nsICachingChannel);
      if (channel) {
        var entryInfo = channel.cacheToken.QueryInterface(Ci.nsICacheEntryInfo);
        if (entryInfo) {
          // nsICacheEntryInfo returns value as seconds,
          // expireTime stores as milliseconds
          var expireTime = entryInfo.expirationTime * 1000;
          var nowTime = Date.now();

          // note, expireTime can be 0, see bug 383538
          if (expireTime > nowTime) {
            this._setResourceTTL(Math.max((expireTime - nowTime),
                                 gExpiration));
            return;
          }
        }
      }
    }
    catch (ex) { }
    this._setResourceTTL(this._ttl);
  },

  _setResourceTTL: function LLL__setResourceTTL(aMilliseconds) {
    var expireTime = Date.now() + aMilliseconds;
    ans.setItemAnnotation(this._livemark.folderId, LMANNO_EXPIRATION,
                          expireTime, 0, ans.EXPIRE_NEVER);
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
  , Ci.nsINavHistoryBatchCallback
  , Ci.nsIBadCertListener2
  , Ci.nsISSLErrorListener
  , Ci.nsIInterfaceRequestor
  ])
}

let component = [LivemarkService];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
