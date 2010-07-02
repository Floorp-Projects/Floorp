/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is mozilla.org
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

var EXPORTED_SYMBOLS = [ "AddonRepository" ];

const PREF_GETADDONS_BROWSEADDONS        = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";

const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const API_VERSION = "1.2";

function AddonSearchResult(aId) {
  this.id = aId;
  this.screenshots = [];
}

AddonSearchResult.prototype = {
  /**
   * The ID of the add-on
   */
  id: null,

  /**
   * The name of the add-on
   */
  name: null,

  /**
   * The version of the add-on
   */
  version: null,

  /**
   * A short description of the add-on
   */
  description: null,

  /**
   * The full description of the add-on
   */
  fullDescription: null,

  /**
   * The rating of the add-on, 0-5 or -1 if unrated
   */
  rating: -1,

  /**
   * The url of the add-ons icon or empty if there is no icon
   */
  iconURL: null,

  /**
   * An array of screenshot urls for the add-on
   */
  screenshots: null,

  /**
   * The homepage for the add-on
   */
  homepageURL: null,

  /**
   * The add-on type (e.g. "extension" or "theme")
   */
  type: null,

  /**
   * AddonInstall object generated from the add-on XPI url
   */
  install: null,

  /**
   * True or false depending on whether the add-on is compatible with the
   * current version and platform of the application
   */
  isCompatible: true,

  /**
   * True if the add-on has a secure means of updating
   */
  providesUpdatesSecurely: true,

  /**
   * The current blocklist state of the add-on
   */
  blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,

  /**
   * True if this add-on cannot be used in the application based on version
   * compatibility, dependencies and blocklisting
   */
  appDisabled: false,

  /**
   * True if the user wants this add-on to be disabled
   */
  userDisabled: false,

  /**
   * Indicates what scope the add-on is installed in, per profile, user,
   * system or application
   */
  scope: AddonManager.SCOPE_PROFILE,

  /**
   * True if the add-on is currently functional
   */
  isActive: true,

  /**
   * The creator of the add-on
   */
  creator: null,

  /**
   * A bitfield holding all of the current operations that are waiting to be
   * performed for this add-on
   */
  pendingOperations: AddonManager.PENDING_NONE,

  /**
   * A bitfield holding all the the operations that can be performed on
   * this add-on
   */
  permissions: 0
}

/**
 * The add-on repository is a source of add-ons that can be installed. It can
 * be searched in two ways. One returns a list of add-ons that come highly
 * recommended, this list should change frequently. The other way is to
 * search for specific search terms entered by the user. Searches are
 * asynchronous and results should be passed to the provided callback object
 * when complete. The results passed to the callback should only include add-ons
 * that are compatible with the current application and are not already
 * installed. Searches are always asynchronous and should be passed to the
 * callback object provided.
 */
var AddonRepository = {
  // The current set of results
  _results: null,

  // Whether we are currently searching or not
  _searching: false,

  // Is this a search for recommended add-ons
  _recommended: false,

  // XHR associated with the current request
  _request: null,

  /*
   * Addon search results callback object that contains two functions
   *
   * searchSucceeded - Called when a search has suceeded.
   *
   * @param  aAddons        an array of the add-on results. In the case of
   *                        searching for specific terms the ordering of results
   *                        may be determined by the search provider.
   * @param  aAddonCount    The length of aAddons
   * @param  aTotalResults  The total results actually available in the
   *                        repository
   *
   *
   * searchFailed - Called when an error occurred when performing a search.
   */
  _callback: null,

  // Maximum number of results to return
  _maxResults: null,

  /**
   * The homepage for visiting this repository. This may be null or an empty
   * string.
   */
  get homepageURL() {
    return Cc["@mozilla.org/toolkit/URLFormatterService;1"].
           getService(Ci.nsIURLFormatter).
           formatURLPref(PREF_GETADDONS_BROWSEADDONS);
  },

  /**
   * Returns whether this instance is currently performing a search. New
   * searches will not be performed while this is the case.
   */
  get isSearching() {
    return this._searching;
  },

  /**
   * The url that can be visited to see recommended add-ons in this repository.
   */
  getRecommendedURL: function() {
    var urlf = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
               getService(Ci.nsIURLFormatter);

    return urlf.formatURLPref(PREF_GETADDONS_BROWSERECOMMENDED);
  },

  /**
   * Retrieves the url that can be visited to see search results for the given
   * terms.
   *
   * @param  aSearchTerms  search terms used to search the repository
   */
  getSearchURL: function(aSearchTerms) {
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var urlf = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
               getService(Ci.nsIURLFormatter);

    var url = prefs.getCharPref(PREF_GETADDONS_BROWSESEARCHRESULTS);
    url = url.replace(/%TERMS%/g, encodeURIComponent(aSearchTerms));
    return urlf.formatURL(url);
  },

  /**
   * Cancels the search in progress. If there is no search in progress this
   * does nothing.
   */
  cancelSearch: function() {
    this._searching = false;
    if (this._request) {
      this._request.abort();
      this._request = null;
    }
    this._callback = null;
    this._results = null;
  },

  /**
   * Begins a search for recommended add-ons in this repository. Results will
   * be passed to the given callback.
   *
   * @param  aMaxResults  the maximum number of results to return
   * @param  aCallback    the callback to pass results to
   */
  retrieveRecommendedAddons: function(aMaxResults, aCallback) {
    if (this._searching)
      return;

    this._searching = true;
    this._results = [];
    this._callback = aCallback;
    this._recommended = true;
    this._maxResults = aMaxResults;

    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var urlf = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
               getService(Ci.nsIURLFormatter);

    var uri = prefs.getCharPref(PREF_GETADDONS_GETRECOMMENDED);
    uri = uri.replace(/%API_VERSION%/g, API_VERSION);
    uri = urlf.formatURL(uri);
    this._loadList(uri);
  },

  /**
   * Begins a search for add-ons in this repository. Results will be passed to
   * the given callback.
   *
   * @param  aSearchTerms  the terms to search for
   * @param  aMaxResults   the maximum number of results to return
   * @param  aCallback     the callback to pass results to
   */
  searchAddons: function(aSearchTerms, aMaxResults, aCallback) {
    if (this._searching)
      return;

    this._searching = true;
    this._results = [];
    this._callback = aCallback;
    this._recommended = false;
    this._maxResults = aMaxResults;

    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var urlf = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
               getService(Ci.nsIURLFormatter);

    var uri = prefs.getCharPref(PREF_GETADDONS_GETSEARCHRESULTS);
    uri = uri.replace(/%API_VERSION%/g, API_VERSION);
    // We double encode due to bug 427155
    uri = uri.replace(/%TERMS%/g, encodeURIComponent(encodeURIComponent(aSearchTerms)));
    uri = urlf.formatURL(uri);
    this._loadList(uri);
  },

  // Posts results to the callback
  _reportSuccess: function(aTotalResults) {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    var addons = [result.addon for each(result in this._results)];
    var callback = this._callback;
    this._callback = null;
    this._results = null;
    callback.searchSucceeded(addons, addons.length, this._recommended ? -1 : aTotalResults);
  },

  // Notifies the callback of a failure
  _reportFailure: function() {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    var callback = this._callback;
    this._callback = null;
    this._results = null;
    callback.searchFailed();
  },

  // Parses an add-on entry from an <addon> element
  _parseAddon: function(aElement, aSkip) {
    var app = Cc["@mozilla.org/xre/app-info;1"].
              getService(Ci.nsIXULAppInfo).
              QueryInterface(Ci.nsIXULRuntime);

    var guidList = aElement.getElementsByTagName("guid");
    if (guidList.length != 1)
      return;

    var guid = guidList[0].textContent.trim();

    // Ignore add-ons already seen in the results
    for (var i = 0; i < this._results.length; i++)
      if (this._results[i].addon.id == guid)
        return;

    // Ignore installed add-ons
    if (aSkip.ids.indexOf(guid) != -1)
      return;

    // Ignore sandboxed add-ons
    var status = aElement.getElementsByTagName("status");
    // The status element has a unique id for each status type. 4 is Public.
    if (status.length != 1 || status[0].getAttribute("id") != 4)
      return;

    // Ignore add-ons not compatible with this OS
    var osList = aElement.getElementsByTagName("compatible_os");
    // Only the version 0 schema included compatible_os if it isn't there then
    // we will see os compatibility on the install elements.
    if (osList.length > 0) {
      var compatible = false;
      var i = 0;
      while (i < osList.length && !compatible) {
        var os = osList[i].textContent.trim();
        if (os == "ALL" || os == app.OS) {
          compatible = true;
          break;
        }
        i++;
      }
      if (!compatible)
        return;
    }

    // Ignore add-ons not compatible with this Application
    compatible = false;
    var tags = aElement.getElementsByTagName("compatible_applications");
    if (tags.length != 1)
      return;
    var vc = Cc["@mozilla.org/xpcom/version-comparator;1"].
             getService(Ci.nsIVersionComparator);
    var apps = tags[0].getElementsByTagName("appID");
    var i = 0;
    while (i < apps.length) {
      if (apps[i].textContent.trim() == app.ID) {
        var parent = apps[i].parentNode;
        var minversion = parent.getElementsByTagName("min_version")[0].textContent.trim();
        var maxversion = parent.getElementsByTagName("max_version")[0].textContent.trim();
        if ((vc.compare(minversion, app.version) > 0) ||
            (vc.compare(app.version, maxversion) > 0))
          return;
        compatible = true;
        break;
      }
      i++;
    }
    if (!compatible)
      return;

    var addon = new AddonSearchResult(guid);
    var result = {
      addon: addon,
      xpiURL: null,
      xpiHash: null
    };
    var node = aElement.firstChild;
    while (node) {
      if (node instanceof Ci.nsIDOMElement) {
        switch (node.localName) {
          case "name":
          case "version":
            addon[node.localName] = node.textContent.trim();
            break;
          case "summary":
            addon.description = node.textContent.trim();
            break;
          case "description":
            addon.fullDescription = node.textContent.trim();
            break;
          case "rating":
            if (node.textContent.length > 0) {
              var rating = parseInt(node.textContent);
              if (rating >= 0)
                addon.rating = Math.min(5, rating);
            }
            break;
          case "thumbnail":
            addon.screenshots.push(node.textContent.trim());
            break;
          case "icon":
            addon.iconURL = node.textContent.trim();
            break;
          case "learnmore":
            addon.homepageURL = node.textContent.trim();
            break;
          case "type":
            // The type element has an id attribute that is the id from AMO's
            // database. This doesn't match our type values to perform a mapping
            addon.type = (node.getAttribute("id") == 2) ? "theme" : "extension";
            break;
          case "install":
            // No os attribute means the xpi is compatible with any os
            if (node.hasAttribute("os")) {
              var os = node.getAttribute("os").toLowerCase();
              // If the os is not ALL and not the current OS then ignore this xpi
              if (os != "all" && os != app.OS.toLowerCase())
                break;
            }
            result.xpiURL = node.textContent.trim();

            // Ignore add-on installs
            if (aSkip.sourceURLs.indexOf(result.xpiURL) != -1)
              return;

            result.xpiHash = node.hasAttribute("hash") ? node.getAttribute("hash") : null;
            break;
        }
      }
      node = node.nextSibling;
    }

    // Add only if there was an xpi compatible with this os
    if (result.xpiURL)
      this._results.push(result);
  },

  _parseAddons: function(aElements, aTotalResults, aSkip) {
    for (var i = 0; i < aElements.length && this._results.length < this._maxResults; i++)
      this._parseAddon(aElements[i], aSkip);

    var pendingResults = this._results.length;
    if (pendingResults == 0) {
      this._reportSuccess(aTotalResults);
      return;
    }

    var self = this;
    this._results.forEach(function(aResult) {
      var addon = aResult.addon;
      var callback = function(aInstall) {
        addon.install = aInstall;
        pendingResults--;
        if (pendingResults == 0)
          self._reportSuccess(aTotalResults);
      }

      AddonManager.getInstallForURL(aResult.xpiURL, callback,
                                    "application/x-xpinstall", aResult.xpiHash,
                                    addon.name, addon.iconURL, addon.version);
    });
  },

  // Called when a single request has completed, parses out any add-ons and
  // either notifies the callback or does a new request for more results
  _listLoaded: function(aEvent) {
    var request = aEvent.target;
    var responseXML = request.responseXML;

    if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
        (request.status != 200 && request.status != 0)) {
      this._reportFailure();
      return;
    }

    var elements = responseXML.documentElement.getElementsByTagName("addon");
    if (responseXML.documentElement.hasAttribute("total_results"))
      var totalResults = responseXML.documentElement.getAttribute("total_results");
    else
      var totalResults = elements.length;

    var self = this;
    var skip = {ids: null, sourceURLs: null};

    AddonManager.getAllAddons(function(aAddons) {
      skip.ids  = [a.id for each (a in aAddons)];
      if (skip.sourceURLs)
        self._parseAddons(elements, totalResults, skip);
    });

    AddonManager.getAllInstalls(function(aInstalls) {
      skip.sourceURLs = [];
      aInstalls.forEach(function(aInstall) {
        if (aInstall.state != AddonManager.STATE_AVAILABLE)
          skip.sourceURLs.push(aInstall.sourceURL);
      });

      if (skip.ids)
        self._parseAddons(elements, totalResults, skip);
    });
  },

  // Performs a new request for results
  _loadList: function(aURI) {
    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    this._request.open("GET", aURI, true);
    this._request.overrideMimeType("text/xml");

    var self = this;
    this._request.onerror = function(aEvent) { self._reportFailure(); };
    this._request.onload = function(aEvent) { self._listLoaded(aEvent); };
    this._request.send(null);
  }
}

