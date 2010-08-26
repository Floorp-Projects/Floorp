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
#   Ben Parr <bparr@bparr.com>
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
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

var EXPORTED_SYMBOLS = [ "AddonRepository" ];

const PREF_GETADDONS_BROWSEADDONS        = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BYIDS               = "extensions.getAddons.get.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";

const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const API_VERSION = "1.5";

// A map between XML keys to AddonSearchResult keys for string values
// that require no extra parsing from XML
const STRING_KEY_MAP = {
  name:               "name",
  version:            "version",
  summary:            "description",
  description:        "fullDescription",
  developer_comments: "developerComments",
  eula:               "eula",
  icon:               "iconURL",
  homepage:           "homepageURL",
  support:            "supportURL"
};

// A map between XML keys to AddonSearchResult keys for integer values
// that require no extra parsing from XML
const INTEGER_KEY_MAP = {
  total_downloads:  "totalDownloads",
  weekly_downloads: "weeklyDownloads",
  daily_users:      "dailyUsers"
};


function AddonSearchResult(aId) {
  this.id = aId;
  this.screenshots = [];
  this.developers = [];
}

AddonSearchResult.prototype = {
  /**
   * The ID of the add-on
   */
  id: null,

  /**
   * The add-on type (e.g. "extension" or "theme")
   */
  type: null,

  /**
   * The name of the add-on
   */
  name: null,

  /**
   * The version of the add-on
   */
  version: null,

  /**
   * The creator of the add-on
   */
  creator: null,

  /**
   * The developers of the add-on
   */
  developers: null,

  /**
   * A short description of the add-on
   */
  description: null,

  /**
   * The full description of the add-on
   */
  fullDescription: null,

  /**
   * The developer comments for the add-on. This includes any information
   * that may be helpful to end users that isn't necessarily applicable to
   * the add-on description (e.g. known major bugs)
   */
  developerComments: null,

  /**
   * The end-user licensing agreement (EULA) of the add-on
   */
  eula: null,

  /**
   * The url of the add-on's icon
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
   * The support URL for the add-on
   */
  supportURL: null,

  /**
   * The contribution url of the add-on
   */
  contributionURL: null,

  /**
   * The suggested contribution amount
   */
  contributionAmount: null,

  /**
   * The rating of the add-on, 0-5
   */
  averageRating: null,

  /**
   * The number of reviews for this add-on
   */
  reviewCount: null,

  /**
   * The URL to the list of reviews for this add-on
   */
  reviewURL: null,

  /**
   * The total number of times the add-on was downloaded
   */
  totalDownloads: null,

  /**
   * The number of times the add-on was downloaded the current week
   */
  weeklyDownloads: null,

  /**
   * The number of daily users for the add-on
   */
  dailyUsers: null,

  /**
   * AddonInstall object generated from the add-on XPI url
   */
  install: null,

  /**
   * nsIURI storing where this add-on was installed from
   */
  sourceURI: null,

  /**
   * The status of the add-on in the repository (e.g. 4 = "Public")
   */
  repositoryStatus: null,

  /**
   * The size of the add-on's files in bytes. For an add-on that have not yet
   * been downloaded this may be an estimated value.
   */
  size: null,

  /**
   * The Date that the add-on was most recently updated
   */
  updateDate: null,

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
   * A bitfield holding all of the current operations that are waiting to be
   * performed for this add-on
   */
  pendingOperations: AddonManager.PENDING_NONE,

  /**
   * A bitfield holding all the the operations that can be performed on
   * this add-on
   */
  permissions: 0,

  /**
   * Tests whether this add-on is known to be compatible with a
   * particular application and platform version.
   *
   * @param  appVersion
   *         An application version to test against
   * @param  platformVersion
   *         A platform version to test against
   * @return Boolean representing if the add-on is compatible
   */
  isCompatibleWith: function(aAppVerison, aPlatformVersion) {
    return true;
  },

  /**
   * Starts an update check for this add-on. This will perform
   * asynchronously and deliver results to the given listener.
   *
   * @param  aListener
   *         An UpdateListener for the update process
   * @param  aReason
   *         A reason code for performing the update
   * @param  aAppVersion
   *         An application version to check for updates for
   * @param  aPlatformVersion
   *         A platform version to check for updates for
   */
  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in aListener)
      aListener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in aListener)
      aListener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in aListener)
      aListener.onUpdateFinished(this);
  }
}

function AddonAuthor(aName, aURL) {
  this.name = aName;
  this.url = aURL;
}

AddonAuthor.prototype = {
  /**
   * The name of the author
   */
  name: null,

  /**
   * The URL of the author's profile page
   */
  url: null,

  /**
   * Returns the author's name, defaulting to the empty string
   */
  toString: function() {
    return this.name || "";
  }
}

function AddonScreenshot(aURL, aThumbnailURL, aCaption) {
  this.url = aURL;
  this.thumbnailURL = aThumbnailURL;
  this.caption = aCaption;
}

AddonScreenshot.prototype = {
  /**
   * The URL to the full version of the screenshot
   */
  url: null,

  /**
   * The URL to the thumbnail version of the screenshot
   */
  thumbnailURL: null,

  /**
   * The caption of the screenshot
   */
  caption: null,

  /**
   * Returns the screenshot URL, defaulting to the empty string
   */
  toString: function() {
    return this.url || "";
  }
}

/**
 * The add-on repository is a source of add-ons that can be installed. It can
 * be searched in three ways. The first takes a list of IDs and returns a
 * list of the corresponding add-ons. The second returns a list of add-ons that
 * come highly recommended. This list should change frequently. The third is to
 * search for specific search terms entered by the user. Searches are
 * asynchronous and results should be passed to the provided callback object
 * when complete. The results passed to the callback should only include add-ons
 * that are compatible with the current application and are not already
 * installed.
 */
var AddonRepository = {
  // Whether a search is currently in progress
  _searching: false,

  // XHR associated with the current request
  _request: null,

  /*
   * Addon search results callback object that contains two functions
   *
   * searchSucceeded - Called when a search has suceeded.
   *
   * @param  aAddons
   *         An array of the add-on results. In the case of searching for
   *         specific terms the ordering of results may be determined by
   *         the search provider.
   * @param  aAddonCount
   *         The length of aAddons
   * @param  aTotalResults
   *         The total results actually available in the repository
   *
   *
   * searchFailed - Called when an error occurred when performing a search.
   */
  _callback: null,

  // Maximum number of results to return
  _maxResults: null,

  /**
   * The homepage for visiting this repository. If the corresponding preference
   * is not defined, defaults to about:blank.
   */
  get homepageURL() {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSEADDONS, {});
    return (url != null) ? url : "about:blank";
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
   * If the corresponding preference is not defined, defaults to about:blank.
   */
  getRecommendedURL: function() {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSERECOMMENDED, {});
    return (url != null) ? url : "about:blank";
  },

  /**
   * Retrieves the url that can be visited to see search results for the given
   * terms. If the corresponding preference is not defined, defaults to
   * about:blank.
   *
   * @param  aSearchTerms
   *         Search terms used to search the repository
   */
  getSearchURL: function(aSearchTerms) {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSESEARCHRESULTS, {
      TERMS : encodeURIComponent(aSearchTerms)
    });
    return (url != null) ? url : "about:blank";
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
  },

  /**
   * Begins a search for add-ons in this repository by ID. Results will be
   * passed to the given callback.
   *
   * @param  aIDs
   *         The array of ids to search for
   * @param  aCallback
   *         The callback to pass results to
   */
  getAddonsByIDs: function(aIDs, aCallback) {
    let url = this._formatURLPref(PREF_GETADDONS_BYIDS, {
      API_VERSION : API_VERSION,
      IDS : aIDs.map(encodeURIComponent).join(',')
    });

    let self = this;
    function handleResults(aElements, aTotalResults) {
      // Don't use this._parseAddons() so that, for example,
      // incompatible add-ons are not filtered out
      let results = [];
      for (let i = 0; i < aElements.length && results.length < self._maxResults; i++) {
        let result = self._parseAddon(aElements[i]);
        if (result == null)
          continue;

        // Ignore add-on if it wasn't actually requested
        let idIndex = aIDs.indexOf(result.addon.id);
        if (idIndex == -1)
          continue;

        results.push(result);
        // Ignore this add-on from now on
        aIDs.splice(idIndex, 1);
      }

      // aTotalResults irrelevant
      self._reportSuccess(results, -1);
    }

    this._beginSearch(url, aIDs.length, aCallback, handleResults);
  },

  /**
   * Begins a search for recommended add-ons in this repository. Results will
   * be passed to the given callback.
   *
   * @param  aMaxResults
   *         The maximum number of results to return
   * @param  aCallback
   *         The callback to pass results to
   */
  retrieveRecommendedAddons: function(aMaxResults, aCallback) {
    let url = this._formatURLPref(PREF_GETADDONS_GETRECOMMENDED, {
      API_VERSION : API_VERSION,

      // Get twice as many results to account for potential filtering
      MAX_RESULTS : 2 * aMaxResults
    });

    let self = this;
    function handleResults(aElements, aTotalResults) {
      self._getLocalAddonIds(function(aLocalAddonIds) {
        // aTotalResults irrelevant
        self._parseAddons(aElements, -1, aLocalAddonIds);
      });
    }

    this._beginSearch(url, aMaxResults, aCallback, handleResults);
  },

  /**
   * Begins a search for add-ons in this repository. Results will be passed to
   * the given callback.
   *
   * @param  aSearchTerms
   *         The terms to search for
   * @param  aMaxResults
   *         The maximum number of results to return
   * @param  aCallback
   *         The callback to pass results to
   */
  searchAddons: function(aSearchTerms, aMaxResults, aCallback) {
    let url = this._formatURLPref(PREF_GETADDONS_GETSEARCHRESULTS, {
      API_VERSION : API_VERSION,
      TERMS : encodeURIComponent(aSearchTerms),

      // Get twice as many results to account for potential filtering
      MAX_RESULTS : 2 * aMaxResults
    });

    let self = this;
    function handleResults(aElements, aTotalResults) {
      self._getLocalAddonIds(function(aLocalAddonIds) {
        self._parseAddons(aElements, aTotalResults, aLocalAddonIds);
      });
    }

    this._beginSearch(url, aMaxResults, aCallback, handleResults);
  },

  // Posts results to the callback
  _reportSuccess: function(aResults, aTotalResults) {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let addons = [result.addon for each(result in aResults)];
    let callback = this._callback;
    this._callback = null;
    callback.searchSucceeded(addons, addons.length, aTotalResults);
  },

  // Notifies the callback of a failure
  _reportFailure: function() {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let callback = this._callback;
    this._callback = null;
    callback.searchFailed();
  },

  // Get descendant by unique tag name. Returns null if not unique tag name.
  _getUniqueDescendant: function(aElement, aTagName) {
    let elementsList = aElement.getElementsByTagName(aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },

  // Parse out trimmed text content. Returns null if text content empty.
  _getTextContent: function(aElement) {
    let textContent = aElement.textContent.trim();
    return (textContent.length > 0) ? textContent : null;
  },

  // Parse out trimmed text content of a descendant with the specified tag name
  // Returns null if the parsing unsuccessful.
  _getDescendantTextContent: function(aElement, aTagName) {
    let descendant = this._getUniqueDescendant(aElement, aTagName);
    return (descendant != null) ? this._getTextContent(descendant) : null;
  },

  /*
   * Creates an AddonSearchResult by parsing an <addon> element
   *
   * @param  aElement
   *         The <addon> element to parse
   * @param  aSkip
   *         Object containing ids and sourceURIs of add-ons to skip.
   * @return Result object containing the parsed AddonSearchResult, xpiURL and
   *         xpiHash if the parsing was successful. Otherwise returns null.
   */
  _parseAddon: function(aElement, aSkip) {
    let skipIDs = (aSkip && aSkip.ids) ? aSkip.ids : [];
    let skipSourceURIs = (aSkip && aSkip.sourceURIs) ? aSkip.sourceURIs : [];

    let guid = this._getDescendantTextContent(aElement, "guid");
    if (guid == null || skipIDs.indexOf(guid) != -1)
      return null;

    let addon = new AddonSearchResult(guid);
    let result = {
      addon: addon,
      xpiURL: null,
      xpiHash: null
    };

    let self = this;
    for (let node = aElement.firstChild; node; node = node.nextSibling) {
      if (!(node instanceof Ci.nsIDOMElement))
        continue;

      let localName = node.localName;

      // Handle case where the wanted string value is located in text content
      if (localName in STRING_KEY_MAP) {
        addon[STRING_KEY_MAP[localName]] = this._getTextContent(node);
        continue;
      }

      // Handle case where the wanted integer value is located in text content
      if (localName in INTEGER_KEY_MAP) {
        let value = parseInt(this._getTextContent(node));
        if (value >= 0)
          addon[INTEGER_KEY_MAP[localName]] = value;
        continue;
      }

      // Handle cases that aren't as simple as grabbing the text content
      switch (localName) {
        case "type":
          // Map AMO's type id to corresponding string
          let id = parseInt(node.getAttribute("id"));
          switch (id) {
            case 1:
              addon.type = "extension";
              break;
            case 2:
              addon.type = "theme";
              break;
            default:
              Cu.reportError("Unknown type id when parsing addon: " + id);
          }
          break;
        case "authors":
          let authorNodes = node.getElementsByTagName("author");
          Array.forEach(authorNodes, function(aAuthorNode) {
            let name = self._getDescendantTextContent(aAuthorNode, "name");
            let link = self._getDescendantTextContent(aAuthorNode, "link");
            if (name == null || link == null)
              return;

            let author = new AddonAuthor(name, link);
            if (addon.creator == null)
              addon.creator = author;
            else
              addon.developers.push(author);
          });
          break;
        case "previews":
          let previewNodes = node.getElementsByTagName("preview");
          Array.forEach(previewNodes, function(aPreviewNode) {
            let full = self._getDescendantTextContent(aPreviewNode, "full");
            if (full == null)
              return;

            let thumbnail = self._getDescendantTextContent(aPreviewNode, "thumbnail");
            let caption = self._getDescendantTextContent(aPreviewNode, "caption");
            let screenshot = new AddonScreenshot(full, thumbnail, caption);

            if (aPreviewNode.getAttribute("primary") == 1)
              addon.screenshots.unshift(screenshot);
            else
              addon.screenshots.push(screenshot);
          });
          break;
        case "learnmore":
          addon.homepageURL = addon.homepageURL || this._getTextContent(node);
          break;
        case "contribution_data":
          let meetDevelopers = this._getDescendantTextContent(node, "meet_developers");
          let suggestedAmount = this._getDescendantTextContent(node, "suggested_amount");
          if (meetDevelopers != null && suggestedAmount != null) {
            addon.contributionURL = meetDevelopers;
            addon.contributionAmount = suggestedAmount;
          }
          break
        case "rating":
          let averageRating = parseInt(this._getTextContent(node));
          if (averageRating >= 0)
            addon.averageRating = Math.min(5, averageRating);
          break;
        case "reviews":
          let url = this._getTextContent(node);
          let num = parseInt(node.getAttribute("num"));
          if (url != null && num >= 0) {
            addon.reviewURL = url;
            addon.reviewCount = num;
          }
          break;
        case "status":
          let repositoryStatus = parseInt(node.getAttribute("id"));
          if (!isNaN(repositoryStatus))
            addon.repositoryStatus = repositoryStatus;
          break;
        case "install":
          // No os attribute means the xpi is compatible with any os
          if (node.hasAttribute("os")) {
            let os = node.getAttribute("os").trim().toLowerCase();
            // If the os is not ALL and not the current OS then ignore this xpi
            if (os != "all" && os != Services.appinfo.OS.toLowerCase())
              break;
          }

          let xpiURL = this._getTextContent(node);
          if (xpiURL == null)
            break;

          if (skipSourceURIs.indexOf(xpiURL) != -1)
            return null;

          result.xpiURL = xpiURL;
          addon.sourceURI = NetUtil.newURI(xpiURL);

          let size = parseInt(node.getAttribute("size"));
          addon.size = (size >= 0) ? size : null;

          let xpiHash = node.getAttribute("hash");
          if (xpiHash != null)
            xpiHash = xpiHash.trim();
          result.xpiHash = xpiHash ? xpiHash : null;
          break;
        case "last_updated":
          let epoch = parseInt(node.getAttribute("epoch"));
          if (!isNaN(epoch))
            addon.updateDate = new Date(1000 * epoch);
          break;
      }
    }

    return result;
  },

  _parseAddons: function(aElements, aTotalResults, aSkip) {
    let self = this;
    let results = [];
    for (let i = 0; i < aElements.length && results.length < this._maxResults; i++) {
      let element = aElements[i];

      // Ignore sandboxed add-ons
      let status = this._getUniqueDescendant(element, "status");
      // The status element has a unique id for each status type. 4 is Public.
      if (status == null || status.getAttribute("id") != 4)
        continue;

      // Ignore add-ons not compatible with this Application
      let tags = this._getUniqueDescendant(element, "compatible_applications");
      if (tags == null)
        continue;

      let applications = tags.getElementsByTagName("appID");
      let compatible = Array.some(applications, function(aAppNode) {
        if (self._getTextContent(aAppNode) != Services.appinfo.ID)
          return false;

        let parent = aAppNode.parentNode;
        let minVersion = self._getDescendantTextContent(parent, "min_version");
        let maxVersion = self._getDescendantTextContent(parent, "max_version");
        if (minVersion == null || maxVersion == null)
          return false;

        let currentVersion = Services.appinfo.version;
        return (Services.vc.compare(minVersion, currentVersion) <= 0 &&
                Services.vc.compare(currentVersion, maxVersion) <= 0);
      });

      if (!compatible)
        continue;

      // Add-on meets all requirements, so parse out data
      let result = this._parseAddon(element, aSkip);
      if (result == null)
        continue;

      // Ignore add-on missing a required attribute
      let requiredAttributes = ["id", "name", "version", "type", "creator"];
      if (requiredAttributes.some(function(aAttribute) !result.addon[aAttribute]))
        continue;

      // Add only if there was an xpi compatible with this OS
      if (!result.xpiURL)
        continue;

      results.push(result);
      // Ignore this add-on from now on by adding it to the skip array
      aSkip.ids.push(result.addon.id);
    }

    // Immediately report success if no AddonInstall instances to create
    let pendingResults = results.length;
    if (pendingResults == 0) {
      this._reportSuccess(results, aTotalResults);
      return;
    }

    // Create an AddonInstall for each result
    let self = this;
    results.forEach(function(aResult) {
      let addon = aResult.addon;
      let callback = function(aInstall) {
        addon.install = aInstall;
        pendingResults--;
        if (pendingResults == 0)
          self._reportSuccess(results, aTotalResults);
      }

      AddonManager.getInstallForURL(aResult.xpiURL, callback,
                                    "application/x-xpinstall", aResult.xpiHash,
                                    addon.name, addon.iconURL, addon.version);
    });
  },

  // Begins a new search if one isn't currently executing
  _beginSearch: function(aURI, aMaxResults, aCallback, aHandleResults) {
    if (this._searching || aURI == null || aMaxResults <= 0) {
      aCallback.searchFailed();
      return;
    }

    this._searching = true;
    this._callback = aCallback;
    this._maxResults = aMaxResults;

    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    this._request.open("GET", aURI, true);
    this._request.overrideMimeType("text/xml");

    let self = this;
    this._request.onerror = function(aEvent) { self._reportFailure(); };
    this._request.onload = function(aEvent) {
      let request = aEvent.target;
      let responseXML = request.responseXML;

      if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
          (request.status != 200 && request.status != 0)) {
        self._reportFailure();
        return;
      }

      let documentElement = responseXML.documentElement;
      let elements = documentElement.getElementsByTagName("addon");
      let totalResults = elements.length;
      let parsedTotalResults = parseInt(documentElement.getAttribute("total_results"));
      // Parsed value of total results only makes sense if >= elements.length
      if (parsedTotalResults >= totalResults)
        totalResults = parsedTotalResults;

      aHandleResults(elements, totalResults);
    };
    this._request.send(null);
  },

  // Gets the id's of local add-ons, and the sourceURI's of local installs,
  // passing the results to aCallback
  _getLocalAddonIds: function(aCallback) {
    let self = this;
    let localAddonIds = {ids: null, sourceURIs: null};

    AddonManager.getAllAddons(function(aAddons) {
      localAddonIds.ids = [a.id for each (a in aAddons)];
      if (localAddonIds.sourceURIs)
        aCallback(localAddonIds);
    });

    AddonManager.getAllInstalls(function(aInstalls) {
      localAddonIds.sourceURIs = [];
      aInstalls.forEach(function(aInstall) {
        if (aInstall.state != AddonManager.STATE_AVAILABLE)
          localAddonIds.sourceURIs.push(aInstall.sourceURI.spec);
      });

      if (localAddonIds.ids)
        aCallback(localAddonIds);
    });
  },

  // Create url from preference, returning null if preference does not exist
  _formatURLPref: function(aPreference, aSubstitutions) {
    let url = null;
    try {
      url = Services.prefs.getCharPref(aPreference);
    } catch(e) {
      Cu.reportError("_formatURLPref: Couldn't get pref: " + aPreference);
      return null;
    }

    url = url.replace(/%([A-Z_]+)%/g, function(aMatch, aKey) {
      return (aKey in aSubstitutions) ? aSubstitutions[aKey] : aMatch;
    });

    return Services.urlFormatter.formatURL(url);
  }
}

