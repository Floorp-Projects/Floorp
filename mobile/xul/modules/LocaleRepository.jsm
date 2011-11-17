/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozilla.com>
 *  Wes Johnston <wjohnston@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let EXPORTED_SYMBOLS = ["LocaleRepository"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

// A map between XML keys to LocaleSearchResult keys for string values
// that require no extra parsing from XML
const STRING_KEY_MAP = {
  name:               "name",
  target_locale:      "targetLocale",
  version:            "version",
  icon:               "iconURL",
  homepage:           "homepageURL",
  support:            "supportURL",
  strings:            "strings"
};

var LocaleRepository = {
  loggingEnabled: false,

  log: function(aMessage) {
    if (this.loggingEnabled)
      dump(aMessage + "\n");
  },

  _getUniqueDescendant: function _getUniqueDescendant(aElement, aTagName) {
    let elementsList = aElement.getElementsByTagName(aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },
  
  _getTextContent: function _getTextContent(aElement) {
    let textContent = aElement.textContent.trim();
    return (textContent.length > 0) ? textContent : null;
  },
  
  _getDescendantTextContent: function _getDescendantTextContent(aElement, aTagName) {
    let descendant = this._getUniqueDescendant(aElement, aTagName);
    return (descendant != null) ? this._getTextContent(descendant) : null;
  },

  getLocales: function getLocales(aCallback, aFilters) {
    let url = Services.prefs.getCharPref("extensions.getLocales.get.url");

    if (!url) {
      aCallback([]);
      return;
    }

    let buildID = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).QueryInterface(Ci.nsIXULRuntime).appBuildID;
    if (aFilters) {
      if (aFilters.buildID)
        buildID = aFilters.buildID;
    }
    buildID = buildID.substring(0,4) + "-" + buildID.substring(4).replace(/\d{2}(?=\d)/g, "$&-");
    url = url.replace(/%BUILDID_EXPANDED%/g, buildID);
    url = Services.urlFormatter.formatURL(url);

    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("GET", url, true);
    request.overrideMimeType("text/xml");
  
    let self = this;
    request.addEventListener("readystatechange", function () {
      if (request.readyState == 4) {
        if (request.status == 200) {
          self.log("---- got response")
          let documentElement = request.responseXML.documentElement;
          let elements = documentElement.getElementsByTagName("addon");
          let totalResults = elements.length;
          let parsedTotalResults = parseInt(documentElement.getAttribute("total_results"));
          if (parsedTotalResults >= totalResults)
            totalResults = parsedTotalResults;

          // TODO: Create a real Skip object from installed locales
          self._parseLocales(elements, totalResults, { ids: [], sourceURIs: [] }, aCallback);
        } else {
          Cu.reportError("Locale Repository: Error getting locale from AMO [" + request.status + "]");
        }
      }
    }, false);
  
    request.send(null);
  },

  _parseLocale: function _parseLocale(aElement, aSkip) {
    let skipIDs = (aSkip && aSkip.ids) ? aSkip.ids : [];
    let skipSourceURIs = (aSkip && aSkip.sourceURIs) ? aSkip.sourceURIs : [];
  
    let guid = this._getDescendantTextContent(aElement, "guid");
    if (guid == null || skipIDs.indexOf(guid) != -1)
      return null;
  
    let addon = new LocaleSearchResult(guid);
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
      // but only if the content is not empty
      if (localName in STRING_KEY_MAP) {
        addon[STRING_KEY_MAP[localName]] = this._getTextContent(node) || addon[STRING_KEY_MAP[localName]];
        continue;
      }
  
      // Handle cases that aren't as simple as grabbing the text content
      switch (localName) {
        case "type":
          // Map AMO's type id to corresponding string
          let id = parseInt(node.getAttribute("id"));
          switch (id) {
            case 5:
              addon.type = "language";
              break;
            default:
              this.log("Unknown type id when parsing addon: " + id);
          }
          break;
        case "authors":
          let authorNodes = node.getElementsByTagName("author");
          Array.forEach(authorNodes, function(aAuthorNode) {
            let name = self._getDescendantTextContent(aAuthorNode, "name");
            if (name == null)
              name = self._getTextContent(aAuthorNode);
            let link = self._getDescendantTextContent(aAuthorNode, "link");
            if (name == null && link == null)
              return;
  
            let author = { name: name, link: link };
            if (addon.creator == null) {
              addon.creator = author;
            } else {
              if (addon.developers == null)
                addon.developers = [];
  
              addon.developers.push(author);
            }
          });
          break;
        case "status":
          let repositoryStatus = parseInt(node.getAttribute("id"));
          if (!isNaN(repositoryStatus))
            addon.repositoryStatus = repositoryStatus;
          break;
        case "all_compatible_os":
          let nodes = node.getElementsByTagName("os");
          addon.isPlatformCompatible = Array.some(nodes, function(aNode) {
            let text = aNode.textContent.toLowerCase().trim();
            return text == "all" || text == Services.appinfo.OS.toLowerCase();
          });
          break;
        case "install":
          // No os attribute means the xpi is compatible with any os
          if (node.hasAttribute("os") && node.getAttribute("os")) {
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
          try {
            addon.sourceURI = NetUtil.newURI(xpiURL);
          } catch(ex) {
            this.log("Addon has invalid uri: " + addon.sourceURI);
            addon.sourceURI = null;
          }
  
          let size = parseInt(node.getAttribute("size"));
          addon.size = (size >= 0) ? size : null;
  
          let xpiHash = node.getAttribute("hash");
          if (xpiHash != null)
            xpiHash = xpiHash.trim();
          result.xpiHash = xpiHash ? xpiHash : null;
          break;
      }
    }
  
    return result;
  },

  _parseLocales: function _parseLocales(aElements, aTotalResults, aSkip, aCallback) {
    let self = this;
    let results = [];
    for (let i = 0; i < aElements.length; i++) {
      let element = aElements[i];

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
        return (Services.vc.compare(minVersion, currentVersion) <= 0 && Services.vc.compare(currentVersion, maxVersion) <= 0);
      });

      if (!compatible)
        continue;

      // Add-on meets all requirements, so parse out data
      let result = this._parseLocale(element, aSkip);
      if (result == null)
        continue;

      // Ignore add-on missing a required attribute
      let requiredAttributes = ["id", "name", "version", "type", "targetLocale", "sourceURI"];
      if (requiredAttributes.some(function(aAttribute) !result.addon[aAttribute]))
        continue;

      // Add only if the add-on is compatible with the platform
      if (!result.addon.isPlatformCompatible)
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
      aCallback([]);
      return;
    }

    // Create an AddonInstall for each result
    let self = this;
    results.forEach(function(aResult) {
      let addon = aResult.addon;
      let callback = function(aInstall) {
        aResult.addon.install = aInstall;
        pendingResults--;
        if (pendingResults == 0)
          aCallback(results);
      }

      if (aResult.xpiURL) {
        AddonManager.getInstallForURL(aResult.xpiURL, callback,
                                      "application/x-xpinstall", aResult.xpiHash,
                                      addon.name, addon.iconURL, addon.version);
      } else {
        callback(null);
      }
    });
  }
};

function LocaleSearchResult(aId) {
  this.id = aId;
}

LocaleSearchResult.prototype = {
  id: null,
  type: null,
  targetLocale: null,
  name: null,
  addon: null,
  version: null,
  iconURL: null,
  install: null,
  sourceURI: null,
  repositoryStatus: null,
  size: null,
  strings: "",
  updateDate: null,
  isCompatible: true,
  isPlatformCompatible: true,
  providesUpdatesSecurely: true,
  blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  appDisabled: false,
  userDisabled: false,
  scope: AddonManager.SCOPE_PROFILE,
  isActive: true,
  pendingOperations: AddonManager.PENDING_NONE,
  permissions: 0
};
