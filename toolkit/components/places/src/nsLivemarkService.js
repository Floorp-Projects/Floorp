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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com> (C++ author)
 *   Joe Hughes <joe@retrovirus.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Robert Sayre <sayrer@gmail.com> (JS port)
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

#include ../../url-classifier/content/js/lang.js
#include ../../url-classifier/content/moz/lang.js
#include ../../url-classifier/content/moz/observer.js
#include ../../url-classifier/content/moz/alarm.js

function LOG(str) {
  dump("*** " + str + "\n");
}

const LS_CLASSID = Components.ID("{dca61eb5-c7cd-4df1-b0fb-d0722baba251}");
const LS_CLASSNAME = "Livemark Service";
const LS_CONTRACTID = "@mozilla.org/browser/livemark-service;2";

const LIVEMARK_TIMEOUT = 15000; // fire every 15 seconds
const LIVEMARK_ICON_URI = "chrome://browser/skin/places/livemarkItem.png";
const PLACES_BUNDLE_URI = 
  "chrome://browser/locale/places/places.properties";
const DEFAULT_LOAD_MSG = "Live Bookmark loading...";
const DEFAULT_FAIL_MSG = "Live Bookmark feed failed to load.";
const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";
const LMANNO_EXPIRATION = "livemark/expiration";
const LMANNO_BMANNO = "livemark/bookmarkFeedURI";

const NH_CONTRACTID = "@mozilla.org/browser/nav-history-service;1";
const AS_CONTRACTID = "@mozilla.org/browser/annotation-service;1";
const OS_CONTRACTID = "@mozilla.org/observer-service;1";
const SB_CONTRACTID = "@mozilla.org/intl/stringbundle;1";
const IO_CONTRACTID = "@mozilla.org/network/io-service;1";
const BMS_CONTRACTID = "@mozilla.org/browser/nav-bookmarks-service;1";
const FAV_CONTRACTID = "@mozilla.org/browser/favicon-service;1";
const LG_CONTRACTID = "@mozilla.org/network/load-group;1";
const FP_CONTRACTID = "@mozilla.org/feed-processor;1";
const SEC_CONTRACTID = "@mozilla.org/scriptsecuritymanager;1";
const SEC_FLAGS = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;

// Check every hour
const EXPIRATION = 3600000;
// Check every 10 minutes on error
const ERROR_EXPIRATION = 600000;

var gIoService = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);
var gStringBundle;
function GetString(name)
{
  try {
    if (!gStringBundle) {
      var bundleService = Cc[SB_CONTRACTID].getService(); 
      bundleService = bundleService.QueryInterface(Ci.nsIStringBundleService);
      gStringBundle = bundleService.createBundle(PLACES_BUNDLE_URI);
    }

    if (gStringBundle)
      return gStringBundle.GetStringFromName(name);
  } catch (ex) {
    LOG("Exception loading string bundle: " + ex.message);
  }

  return null;
}

var gLivemarkService;
function LivemarkService() {
  
  // [ {folderId:, folderURI:, feedURI:, loadGroup:, locked: } ];
  this._livemarks = [];

  this._iconURI = gIoService.newURI(LIVEMARK_ICON_URI, null, null);
  this._loading = GetString("bookmarksLivemarkLoading") || DEFAULT_LOAD_MSG;
  this._observerServiceObserver =
    new G_ObserverServiceObserver('xpcom-shutdown',
                                  BindToObject(this._shutdown, this),
                                  true /*only once*/);
  new G_Alarm(BindToObject(this._fireTimer, this), LIVEMARK_TIMEOUT, 
              true /* repeat */);

  // this is giving a reentrant getService warning in XPCShell. bug 194568.
  this._ans = Cc[AS_CONTRACTID].getService(Ci.nsIAnnotationService);

  var livemarks = this._ans.getPagesWithAnnotation(LMANNO_FEEDURI, {});
  for (var i = 0; i < livemarks.length; i++) {
    var feedURI =
      gIoService.newURI(
        this._ans.getAnnotationString(livemarks[i], LMANNO_FEEDURI),
        null, null
      );
    var queries = { }, options = { };
    this._history.queryStringToQueries(livemarks[i].spec, queries, {}, options);
    var count = {};
    var folders = queries.value[0].getFolders(count);
    if (!(queries.value.length && queries.value.length == 1) && 
        count.value != 1)
      continue; // invalid folder URI (should identify exactly one folder)
    this._pushLivemark(folders[0], livemarks[i], feedURI);
  }
}

LivemarkService.prototype = {

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

  // returns new length of _livemarks
  _pushLivemark: function LS__pushLivemark(folderId, folderURI, feedURI) {
    return this._livemarks.push({folderId: folderId, folderURI: folderURI,
	                         feedURI: feedURI});
  },

  _getLivemarkIndex: function LS__getLivemarkIndex(folderId) {
    for (var i=0; i < this._livemarks.length; ++i) {
      if (this._livemarks[i].folderId == folderId)
        return i;
    }
    throw Cr.NS_ERROR_INVALID_ARG;
  },

  _shutdown: function LS__shutdown() {
    for (var livemark in this._livemarks) {
      if (livemark.loadGroup) 
        livemark.loadGroup.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  _fireTimer: function LS__fireTimer() {
    for (var i=0; i < this._livemarks.length; ++i) {
      this._updateLivemarkChildren(i, false);
    }
  },

  deleteLivemarkChildren: function LS_deleteLivemarkChildren(folderId) {
    var query = this._history.getNewQuery();
    query.setFolders([folderId], 1);
    var options = this._history.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var result = this._history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    var cc = root.childCount;
    for (var i=0; i < cc; ++i) {
      try {
        var node = root.getChild(i);
        this._ans.removeAnnotation(gIoService.newURI(node.uri, null, null), 
                                   LMANNO_BMANNO);
      }
      catch (ex) {
        // continue
      }
    }

    this._bms.removeFolderChildren(folderId);
  },

  insertLivemarkLoadingItem: function LS_insertLivemarkLoading(bms, folderId) {
    var loadingURI = gIoService.newURI("about:livemark-loading", null, null);
    var id = bms.insertItem(folderId, loadingURI, bms.DEFAULT_INDEX);
    bms.setItemTitle(id, this._loading);
  },

  _updateLivemarkChildren:
  function LS__updateLivemarkChildren(index, forceUpdate) {
    if (this._livemarks[index].locked)
      return;
    
    var livemark = this._livemarks[index];
    livemark.locked = true;
    try {
      // Check the TTL/expiration on this.  If there isn't one,
      // then we assume it's never been loaded.  We perform this
      // check even when the update is being forced, in case the
      // livemark has somehow never been loaded.
      var exprTime = this._ans.getAnnotationInt64(livemark.feedURI,
                                                  LMANNO_EXPIRATION);
      if (!forceUpdate && exprTime > Date.now()) {
	// no need to refresh
	livemark.locked = false;
	return;
      }
    } 
    catch (ex) {
      // This livemark has never been loaded, since it has no expire time.
      this.insertLivemarkLoadingItem(this._bms, livemark.folderId);
    }

    var loadgroup;
    try {
      // Create a load group for the request.  This will allow us to
      // automatically keep track of redirects, so we can always
      // cancel the channel.
      loadgroup = Cc[LG_CONTRACTID].createInstance(Ci.nsILoadGroup);
      var uriChannel = gIoService.newChannel(livemark.feedURI.spec, null, null);
      uriChannel.loadGroup = loadgroup;
      uriChannel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND | 
                              Ci.nsIRequest.VALIDATE_ALWAYS;
      var httpChannel = uriChannel.QueryInterface(Ci.nsIHttpChannel);
      httpChannel.requestMethod = "GET";
      httpChannel.setRequestHeader("X-Moz", "livebookmarks", false);

      // Stream the result to the feed parser with this listener
      var listener = new LivemarkLoadListener(livemark);
      httpChannel.asyncOpen(listener, null);
    }
    catch (ex) {
      livemark.locked = false;
      LOG("exception: " + ex);
      throw ex;
    }
    livemark.loadGroup = loadgroup;
  },

  createLivemark: function LS_createLivemark(folder, name, siteURI,
                                             feedURI, index) {
    // Don't add livemarks to livemarks
    if (this.isLivemark(folder))
      throw Cr.NS_ERROR_INVALID_ARG;
    var [livemarkID, livemarkURI] =
      this._createFolder(this._bms, folder, name, siteURI, feedURI, index);
  
    // kick off http fetch
    this._updateLivemarkChildren(
      this._pushLivemark(livemarkID, livemarkURI, feedURI) - 1,
      false
    );

    return livemarkID;
  },

  createLivemarkFolderOnly:
  function LS_createLivemarkFolderOnly(bms, folder, name, siteURI,
                                       feedURI, index) {
    var [livemarkID, livemarkURI] =
      this._createFolder(bms, folder, name, siteURI, feedURI, index);
    this.insertLivemarkLoadingItem(bms, livemarkID);
    this._pushLivemark(livemarkID, livemarkURI, feedURI);
    
    return livemarkID;
  },

  _createFolder:
  function LS__createFolder(bms, folder, name, siteURI, feedURI, index) {
    var livemarkID = bms.createContainer(folder, name, LS_CONTRACTID, index);
    var livemarkURI = bms.getFolderURI(livemarkID);

    // Add an annotation to map the folder URI to the livemark feed URI
    this._ans.setAnnotationString(livemarkURI, LMANNO_FEEDURI, feedURI.spec, 0,
                                  this._ans.EXPIRE_NEVER);
    // Set the favicon
    var faviconService = Cc[FAV_CONTRACTID].getService(Ci.nsIFaviconService);
    faviconService.setFaviconUrlForPage(livemarkURI, this._iconURI);

    if (siteURI) {
      // Add an annotation to map the folder URI to the livemark site URI
      this._ans.setAnnotationString(livemarkURI, LMANNO_SITEURI, siteURI.spec,
                                    0, this._ans.EXPIRE_NEVER);
    }

    return [livemarkID, livemarkURI];
  },

  isLivemark: function LS_isLivemark(folder) {
    var folderURI = this._bms.getFolderURI(folder);
    return this._ans.hasAnnotation(folderURI, LMANNO_FEEDURI);
  },

  _ensureLivemark: function LS__ensureLivemark(container) {
    if (!this.isLivemark(container)) 
      throw Cr.NS_ERROR_INVALID_ARG;
  },

  /**
  * n.b. -- the body of this method is duplicated in 
  *         /browser/components/places/content/toolbar.xml
  *         to avoid instantiating the livemark service on
  *         startup.
  */
  getSiteURI: function LS_getSiteURI(container) {
    this._ensureLivemark(container);
    var containerURI = this._bms.getFolderURI(container);
    var siteURIString;
    try { 
      siteURIString =
        this._ans.getAnnotationString(containerURI, LMANNO_SITEURI);
    }
    catch (ex) {
      return null;
    }

    return gIoService.newURI(siteURIString, null, null);
  },

  setSiteURI: function LS_setSiteURI(container, siteURI) {
    this._ensureLivemark(container);
    var containerURI = this._bms.getFolderURI(container);
    
    if (!siteURI) {
      this._ans.removeAnnotation(containerURI, LMANNO_SITEURI);
      return;
    }

    this._ans.setAnnotationString(containerURI, LMANNO_SITEURI, siteURI.spec,
                                  0, this._ans.EXPIRE_NEVER);
  },

  getFeedURI: function LS_getFeedURI(container) {
    var containerURI = this._bms.getFolderURI(container);
    return gIoService.newURI(this._ans.getAnnotationString(containerURI, 
                                                           LMANNO_FEEDURI),
                             null, null);
  },

  setFeedURI: function LS_setFeedURI(container, feedURI) {
    if (!feedURI)
      throw Cr.NS_ERROR_INVALID_ARG;
    
    var containerURI = this._bms.getFolderURI(container);
    this._ans.setAnnotationString(containerURI, LMANNO_FEEDURI, feedURI.spec,
                                  0, this._ans.EXPIRE_NEVER);

    // now update our internal table
    var livemarkIndex = this._getLivemarkIndex(container);  
    this._livemarks[livemarkIndex].feedURI = feedURI;
  },

  reloadAllLivemarks: function LS_reloadAllLivemarks() {
    for (var i = 0; i < this._livemarks.length; ++i) {
      this._updateLivemarkChildren(i, true);
    }
  },

  reloadLivemarkFolder: function LS_reloadLivemarkFolder(folderID) {
    var livemarkIndex = this._getLivemarkIndex(folderID);  
    this._updateLivemarkChildren(livemarkIndex, true);
  },

  // nsIRemoteContainer
  onContainerRemoving: function LS_onContainerRemoving(container) {
    var livemarkIndex = this._getLivemarkIndex(container);
    var livemark = this._livemarks[livemarkIndex];
    
    // Remove the annotations that link the folder URI to the 
    // Feed URI and Site URI
    this._ans.removeAnnotation(livemark.folderURI, LMANNO_FEEDURI);
    this._ans.removeAnnotation(livemark.folderURI, LMANNO_SITEURI);
    var stillInUse = false;
    stillInUse = this._livemarks.some(
                 function(mark) { return mark.feedURI.equals(livemark.feedURI) } 
                 );
    if (!stillInUse) {
      // ??? the code in the C++ had "livemark_expiration" as
      // the second arg... that must be wrong
      this._ans.removeAnnotation(livemark.feedURI, LMANNO_EXPIRATION);
    }

    if (livemark.loadGroup) 
      livemark.loadGroup.cancel(Cr.NS_BINDING_ABORTED);
    this._livemarks.splice(livemarkIndex, 1);
    this.deleteLivemarkChildren(container);
  },

  // XXXdietrich seems like the mutability of "place:" URIs is the only reason 
  // this code has to exist, which is kinda high-maintenance. 
  onContainerMoved:
  function LS_onContainerMoved(container, newFolder, newIndex) {
    var index = this._getLivemarkIndex(container);
    
    // Update the annotation that maps the folder URI to the livemark feed URI
    var newURI = this._bms.getFolderURI(container);
    var oldURI = this._livemarks[index].folderURI;
    var feedURIString = this._ans.getAnnotationString(oldURI, LMANNO_FEEDURI);
    this._ans.removeAnnotation(oldURI, LMANNO_FEEDURI);
    this._ans.setAnnotation(newURI, LMANNO_FEEDURI, feedURIString, 0, 
                            this._ans.EXPIRE_NEVER);
    
    // Update the annotation that maps the folder URI to the livemark site URI
    var siteURIString;
    try {
      siteURIString = this._ans.GetAnnotationString(oldURI, LMANNO_SITEURI);
    }
    catch (ex) {
      // will throw if no annotation
    }

    if (siteURIString) {
      this._ans.removeAnnotation(oldURI, LMANNO_SITEURI);
      this._ans.setAnnotation(newURI, LMANNO_SITEURI, siteURIString, 0,
                              this._ans.EXPIRE_NEVER);
    }
  },

  childrenReadOnly: true,

  createInstance: function LS_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  
  QueryInterface: function LS_QueryInterface(iid) {
    if (iid.equals(Ci.nsILivemarkService) ||
        iid.equals(Ci.nsIRemoteContainer) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

function LivemarkLoadListener(livemark) {
  this._livemark = livemark;
  this._processor = null;
  this._isAborted = false;
  this._ttl = EXPIRATION;
  this._ans = Cc[AS_CONTRACTID].getService(Ci.nsIAnnotationService);
}

LivemarkLoadListener.prototype = {

  get _failed() {
    return GetString("bookmarksLivemarkFailed") || DEFAULT_FAIL_MSG;
  },

  abort: function LLL_abort() {
    this._isAborted = true;
  },

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

  /**
   * See nsIFeedResultListener.idl
   */
  handleResult: function LLL_handleResult(result) {
    if (this._isAborted) {
      this._livemark.locked = false;
      return;
    }

    try {  
      // We need this to make sure the item links are safe
      var secMan = Cc[SEC_CONTRACTID].getService(Ci.nsIScriptSecurityManager);
      
      // Clear out any child nodes of the livemark folder, since
      // they're about to be replaced.
      var lmService = Cc[LS_CONTRACTID].getService(Ci.nsILivemarkService);
      this.deleteLivemarkChildren(this._livemark.folderId);

      // Enforce well-formedness because the existing code does
      if (!result || !result.doc || result.bozo) {
        this.insertLivemarkFailedItem(this._livemark.folderId);
        this._ttl = EXPIRATION;
        throw Cr.NS_ERROR_FAILURE;
      }

      var title, href, entry;
      var feed = result.doc.QueryInterface(Ci.nsIFeed);
      // Loop through and check for a link and a title
      // as the old code did
      for (var i = 0; i < feed.items.length; ++i) {
        entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
        if (entry.title)
          title = entry.title.plainText();
        else if (entry.updated)
          title = entry.updated;
        
        if (entry.link) {
          try {
              secMan.checkLoadURIStr(this._livemark.feedURI.spec,
                                     entry.link.spec, SEC_FLAGS);
              href = entry.link;
          } 
          catch (ex) {
          }
        }
        
        if (href && title) {
          this.insertLivemarkChild(this._livemark.folderId,
                                   href, title);
        }
      }
    } 
    finally {
      this._processor.listener = null;
      this._processor = null;
      this._livemark.locked = false;
    }
  },
  
  deleteLivemarkChildren: LivemarkService.prototype.deleteLivemarkChildren,

  insertLivemarkFailedItem: function LS_insertLivemarkFailed(folderId) {
    var failedURI = gIoService.newURI("about:livemark-failed", null, null);
    var id = this._bms.insertItem(folderId, failedURI, this._bms.DEFAULT_INDEX);
    this._bms.setItemTitle(id, this._failed);
  },

  insertLivemarkChild:
  function LS_insertLivemarkChild(folderId, uri, title) {
    var id = this._bms.insertItem(folderId, uri, this._bms.DEFAULT_INDEX);
    this._bms.setItemTitle(id, title);
    this._ans.setAnnotationString(uri, LMANNO_BMANNO, uri.spec, 0,
                                  this._ans.EXPIRE_NEVER);
  },

  /**
   * See nsIStreamListener.idl
   */
  onDataAvailable: function LLL_onDataAvailable(request, context, inputStream, 
						sourceOffset, count) {
    this._processor.onDataAvailable(request, context, inputStream,
                                    sourceOffset, count);
  },
  
  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest: function LLL_onStartRequest(request, context) {
    if (this._isAborted)
      throw Cr.NS_ERROR_UNEXPECTED;

    var channel = request.QueryInterface(Ci.nsIChannel);
 
    // Parse feed data as it comes in
    this._processor = Cc[FP_CONTRACTID].createInstance(Ci.nsIFeedProcessor);
    this._processor.listener = this;
    this._processor.parseAsync(null, channel.URI);
    
    this._processor.onStartRequest(request, context);
  },
  
  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest: function LLL_onStopReqeust(request, context, status) {
    if (!Components.isSuccessCode(status)) {
      // Something went wrong; try to load again in a bit
      this._setResourceTTL(ERROR_EXPIRATION);
      this._isAborted = true;
      return;
    }

    // Set an expiration on the livemark, for reloading the data
    try { 
      this._processor.onStopRequest(request, context, status);

      // Calculate a new ttl
      var channel = request.QueryInterface(Ci.nsICachingChannel);
      if (channel) {
        var entryInfo = channel.cacheToken.QueryInterface(Ci.nsICacheEntryInfo);
        if (entryInfo) {
          var exprtime = entryInfo.expirationTime;
          var nowtime = Date.now();
          
          if (nowtime >= exprtime) {
            expiresTime -= nowtime;
            if (expiresTime > EXPIRATION)
              this._setResourceTTL(expiresTime);
            else
              this._setResourceTTL(EXPIRATION);
          }
        }
      }
    }
    catch (ex) { 
      this._setResourceTTL(this._ttl);
    }
    
  },

  _setResourceTTL: function LLL__setResourceTTL(seconds) {
    var exptime = Date.now() + seconds;
    this._ans.setAnnotationInt64(this._livemark.feedURI,
                                 LMANNO_EXPIRATION, exptime, 0,
                                 Ci.nsIAnnotationService.EXPIRE_NEVER);
  },
  
  /**
   * See nsISupports.idl
   */
  QueryInterface: function LLL_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedResultListener) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver)||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
}

function GenericComponentFactory(ctor) {
  this._ctor = ctor;
}

GenericComponentFactory.prototype = {

  _ctor: null,

  // nsIFactory
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return (new this._ctor()).QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

};

var Module = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIModule) || 
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getClassObject: function M_getClassObject(cm, cid, iid) {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    if (cid.equals(LS_CLASSID))
      return new GenericComponentFactory(LivemarkService);

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  registerSelf: function(cm, file, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
 
    cr.registerFactoryLocation(LS_CLASSID, LS_CLASSNAME,
      LS_CONTRACTID, file, location, type);    
  },

  unregisterSelf: function M_unregisterSelf(cm, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    cr.unregisterFactoryLocation(LS_CLASSID, location);
  },
  
  canUnload: function M_canUnload(cm) {
    return true;
  }
};

function NSGetModule(cm, file) {
  return Module;
}
