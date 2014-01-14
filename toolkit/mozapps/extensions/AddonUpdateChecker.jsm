/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The AddonUpdateChecker is responsible for retrieving the update information
 * from an add-on's remote update manifest.
 */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AddonUpdateChecker" ];

const TIMEOUT               = 60 * 1000;
const PREFIX_NS_RDF         = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const PREFIX_NS_EM          = "http://www.mozilla.org/2004/em-rdf#";
const PREFIX_ITEM           = "urn:mozilla:item:";
const PREFIX_EXTENSION      = "urn:mozilla:extension:";
const PREFIX_THEME          = "urn:mozilla:theme:";
const TOOLKIT_ID            = "toolkit@mozilla.org"
const XMLURI_PARSE_ERROR    = "http://www.mozilla.org/newlayout/xml/parsererror.xml"

const PREF_UPDATE_REQUIREBUILTINCERTS = "extensions.update.requireBuiltInCerts";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/AddonRepository.jsm");

// Shared code for suppressing bad cert dialogs.
XPCOMUtils.defineLazyGetter(this, "CertUtils", function certUtilsLazyGetter() {
  let certUtils = {};
  Components.utils.import("resource://gre/modules/CertUtils.jsm", certUtils);
  return certUtils;
});

var gRDF = Cc["@mozilla.org/rdf/rdf-service;1"].
           getService(Ci.nsIRDFService);

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function logFuncGetter() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.updates", this);
    return this[aName];
  });
}, this);


/**
 * A serialisation method for RDF data that produces an identical string
 * for matching RDF graphs.
 * The serialisation is not complete, only assertions stemming from a given
 * resource are included, multiple references to the same resource are not
 * permitted, and the RDF prolog and epilog are not included.
 * RDF Blob and Date literals are not supported.
 */
function RDFSerializer() {
  this.cUtils = Cc["@mozilla.org/rdf/container-utils;1"].
                getService(Ci.nsIRDFContainerUtils);
  this.resources = [];
}

RDFSerializer.prototype = {
  INDENT: "  ",      // The indent used for pretty-printing
  resources: null,   // Array of the resources that have been found

  /**
   * Escapes characters from a string that should not appear in XML.
   *
   * @param  aString
   *         The string to be escaped
   * @return a string with all characters invalid in XML character data
   *         converted to entity references.
   */
  escapeEntities: function RDFS_escapeEntities(aString) {
    aString = aString.replace(/&/g, "&amp;");
    aString = aString.replace(/</g, "&lt;");
    aString = aString.replace(/>/g, "&gt;");
    return aString.replace(/"/g, "&quot;");
  },

  /**
   * Serializes all the elements of an RDF container.
   *
   * @param  aDs
   *         The RDF datasource
   * @param  aContainer
   *         The RDF container to output the child elements of
   * @param  aIndent
   *         The current level of indent for pretty-printing
   * @return a string containing the serialized elements.
   */
  serializeContainerItems: function RDFS_serializeContainerItems(aDs, aContainer,
                                                                 aIndent) {
    var result = "";
    var items = aContainer.GetElements();
    while (items.hasMoreElements()) {
      var item = items.getNext().QueryInterface(Ci.nsIRDFResource);
      result += aIndent + "<RDF:li>\n"
      result += this.serializeResource(aDs, item, aIndent + this.INDENT);
      result += aIndent + "</RDF:li>\n"
    }
    return result;
  },

  /**
   * Serializes all em:* (see EM_NS) properties of an RDF resource except for
   * the em:signature property. As this serialization is to be compared against
   * the manifest signature it cannot contain the em:signature property itself.
   *
   * @param  aDs
   *         The RDF datasource
   * @param  aResource
   *         The RDF resource that contains the properties to serialize
   * @param  aIndent
   *         The current level of indent for pretty-printing
   * @return a string containing the serialized properties.
   * @throws if the resource contains a property that cannot be serialized
   */
  serializeResourceProperties: function RDFS_serializeResourceProperties(aDs,
                                                                         aResource,
                                                                         aIndent) {
    var result = "";
    var items = [];
    var arcs = aDs.ArcLabelsOut(aResource);
    while (arcs.hasMoreElements()) {
      var arc = arcs.getNext().QueryInterface(Ci.nsIRDFResource);
      if (arc.ValueUTF8.substring(0, PREFIX_NS_EM.length) != PREFIX_NS_EM)
        continue;
      var prop = arc.ValueUTF8.substring(PREFIX_NS_EM.length);
      if (prop == "signature")
        continue;

      var targets = aDs.GetTargets(aResource, arc, true);
      while (targets.hasMoreElements()) {
        var target = targets.getNext();
        if (target instanceof Ci.nsIRDFResource) {
          var item = aIndent + "<em:" + prop + ">\n";
          item += this.serializeResource(aDs, target, aIndent + this.INDENT);
          item += aIndent + "</em:" + prop + ">\n";
          items.push(item);
        }
        else if (target instanceof Ci.nsIRDFLiteral) {
          items.push(aIndent + "<em:" + prop + ">" +
                     this.escapeEntities(target.Value) + "</em:" + prop + ">\n");
        }
        else if (target instanceof Ci.nsIRDFInt) {
          items.push(aIndent + "<em:" + prop + " NC:parseType=\"Integer\">" +
                     target.Value + "</em:" + prop + ">\n");
        }
        else {
          throw Components.Exception("Cannot serialize unknown literal type");
        }
      }
    }
    items.sort();
    result += items.join("");
    return result;
  },

  /**
   * Recursively serializes an RDF resource and all resources it links to.
   * This will only output EM_NS properties and will ignore any em:signature
   * property.
   *
   * @param  aDs
   *         The RDF datasource
   * @param  aResource
   *         The RDF resource to serialize
   * @param  aIndent
   *         The current level of indent for pretty-printing. If undefined no
   *         indent will be added
   * @return a string containing the serialized resource.
   * @throws if the RDF data contains multiple references to the same resource.
   */
  serializeResource: function RDFS_serializeResource(aDs, aResource, aIndent) {
    if (this.resources.indexOf(aResource) != -1 ) {
      // We cannot output multiple references to the same resource.
      throw Components.Exception("Cannot serialize multiple references to " + aResource.Value);
    }
    if (aIndent === undefined)
      aIndent = "";

    this.resources.push(aResource);
    var container = null;
    var type = "Description";
    if (this.cUtils.IsSeq(aDs, aResource)) {
      type = "Seq";
      container = this.cUtils.MakeSeq(aDs, aResource);
    }
    else if (this.cUtils.IsAlt(aDs, aResource)) {
      type = "Alt";
      container = this.cUtils.MakeAlt(aDs, aResource);
    }
    else if (this.cUtils.IsBag(aDs, aResource)) {
      type = "Bag";
      container = this.cUtils.MakeBag(aDs, aResource);
    }

    var result = aIndent + "<RDF:" + type;
    if (!gRDF.IsAnonymousResource(aResource))
      result += " about=\"" + this.escapeEntities(aResource.ValueUTF8) + "\"";
    result += ">\n";

    if (container)
      result += this.serializeContainerItems(aDs, container, aIndent + this.INDENT);

    result += this.serializeResourceProperties(aDs, aResource, aIndent + this.INDENT);

    result += aIndent + "</RDF:" + type + ">\n";
    return result;
  }
}

/**
 * Parses an RDF style update manifest into an array of update objects.
 *
 * @param  aId
 *         The ID of the add-on being checked for updates
 * @param  aUpdateKey
 *         An optional update key for the add-on
 * @param  aRequest
 *         The XMLHttpRequest that has retrieved the update manifest
 * @return an array of update objects
 * @throws if the update manifest is invalid in any way
 */
function parseRDFManifest(aId, aUpdateKey, aRequest) {
  function EM_R(aProp) {
    return gRDF.GetResource(PREFIX_NS_EM + aProp);
  }

  function getValue(aLiteral) {
    if (aLiteral instanceof Ci.nsIRDFLiteral)
      return aLiteral.Value;
    if (aLiteral instanceof Ci.nsIRDFResource)
      return aLiteral.Value;
    if (aLiteral instanceof Ci.nsIRDFInt)
      return aLiteral.Value;
    return null;
  }

  function getProperty(aDs, aSource, aProperty) {
    return getValue(aDs.GetTarget(aSource, EM_R(aProperty), true));
  }

  function getRequiredProperty(aDs, aSource, aProperty) {
    let value = getProperty(aDs, aSource, aProperty);
    if (!value)
      throw Components.Exception("Update manifest is missing a required " + aProperty + " property.");
    return value;
  }

  let rdfParser = Cc["@mozilla.org/rdf/xml-parser;1"].
                  createInstance(Ci.nsIRDFXMLParser);
  let ds = Cc["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].
           createInstance(Ci.nsIRDFDataSource);
  rdfParser.parseString(ds, aRequest.channel.URI, aRequest.responseText);

  // Differentiating between add-on types is deprecated
  let extensionRes = gRDF.GetResource(PREFIX_EXTENSION + aId);
  let themeRes = gRDF.GetResource(PREFIX_THEME + aId);
  let itemRes = gRDF.GetResource(PREFIX_ITEM + aId);
  let addonRes = ds.ArcLabelsOut(extensionRes).hasMoreElements() ? extensionRes
               : ds.ArcLabelsOut(themeRes).hasMoreElements() ? themeRes
               : itemRes;

  // If we have an update key then the update manifest must be signed
  if (aUpdateKey) {
    let signature = getProperty(ds, addonRes, "signature");
    if (!signature)
      throw Components.Exception("Update manifest for " + aId + " does not contain a required signature");
    let serializer = new RDFSerializer();
    let updateString = null;

    try {
      updateString = serializer.serializeResource(ds, addonRes);
    }
    catch (e) {
      throw Components.Exception("Failed to generate signed string for " + aId + ". Serializer threw " + e,
                                 e.result);
    }

    let result = false;

    try {
      let verifier = Cc["@mozilla.org/security/datasignatureverifier;1"].
                     getService(Ci.nsIDataSignatureVerifier);
      result = verifier.verifyData(updateString, signature, aUpdateKey);
    }
    catch (e) {
      throw Components.Exception("The signature or updateKey for " + aId + " is malformed." +
                                 "Verifier threw " + e, e.result);
    }

    if (!result)
      throw Components.Exception("The signature for " + aId + " was not created by the add-on's updateKey");
  }

  let updates = ds.GetTarget(addonRes, EM_R("updates"), true);

  // A missing updates property doesn't count as a failure, just as no avialable
  // update information
  if (!updates) {
    WARN("Update manifest for " + aId + " did not contain an updates property");
    return [];
  }

  if (!(updates instanceof Ci.nsIRDFResource))
    throw Components.Exception("Missing updates property for " + addonRes.Value);

  let cu = Cc["@mozilla.org/rdf/container-utils;1"].
           getService(Ci.nsIRDFContainerUtils);
  if (!cu.IsContainer(ds, updates))
    throw Components.Exception("Updates property was not an RDF container");

  let results = [];
  let ctr = Cc["@mozilla.org/rdf/container;1"].
            createInstance(Ci.nsIRDFContainer);
  ctr.Init(ds, updates);
  let items = ctr.GetElements();
  while (items.hasMoreElements()) {
    let item = items.getNext().QueryInterface(Ci.nsIRDFResource);
    let version = getProperty(ds, item, "version");
    if (!version) {
      WARN("Update manifest is missing a required version property.");
      continue;
    }

    LOG("Found an update entry for " + aId + " version " + version);

    let targetApps = ds.GetTargets(item, EM_R("targetApplication"), true);
    while (targetApps.hasMoreElements()) {
      let targetApp = targetApps.getNext().QueryInterface(Ci.nsIRDFResource);

      let appEntry = {};
      try {
        appEntry.id = getRequiredProperty(ds, targetApp, "id");
        appEntry.minVersion = getRequiredProperty(ds, targetApp, "minVersion");
        appEntry.maxVersion = getRequiredProperty(ds, targetApp, "maxVersion");
      }
      catch (e) {
        WARN(e);
        continue;
      }

      let result = {
        id: aId,
        version: version,
        updateURL: getProperty(ds, targetApp, "updateLink"),
        updateHash: getProperty(ds, targetApp, "updateHash"),
        updateInfoURL: getProperty(ds, targetApp, "updateInfoURL"),
        strictCompatibility: getProperty(ds, targetApp, "strictCompatibility") == "true",
        targetApplications: [appEntry]
      };

      if (result.updateURL && AddonManager.checkUpdateSecurity &&
          result.updateURL.substring(0, 6) != "https:" &&
          (!result.updateHash || result.updateHash.substring(0, 3) != "sha")) {
        WARN("updateLink " + result.updateURL + " is not secure and is not verified" +
             " by a strong enough hash (needs to be sha1 or stronger).");
        delete result.updateURL;
        delete result.updateHash;
      }
      results.push(result);
    }
  }
  return results;
}

/**
 * Starts downloading an update manifest and then passes it to an appropriate
 * parser to convert to an array of update objects
 *
 * @param  aId
 *         The ID of the add-on being checked for updates
 * @param  aUpdateKey
 *         An optional update key for the add-on
 * @param  aUrl
 *         The URL of the update manifest
 * @param  aObserver
 *         An observer to pass results to
 */
function UpdateParser(aId, aUpdateKey, aUrl, aObserver) {
  this.id = aId;
  this.updateKey = aUpdateKey;
  this.observer = aObserver;
  this.url = aUrl;

  this.timer = Cc["@mozilla.org/timer;1"].
               createInstance(Ci.nsITimer);
  this.timer.initWithCallback(this, TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);

  let requireBuiltIn = true;
  try {
    requireBuiltIn = Services.prefs.getBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS);
  }
  catch (e) {
  }

  LOG("Requesting " + aUrl);
  try {
    this.request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                   createInstance(Ci.nsIXMLHttpRequest);
    this.request.open("GET", this.url, true);
    this.request.channel.notificationCallbacks = new CertUtils.BadCertHandler(!requireBuiltIn);
    this.request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to cache.
    this.request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    this.request.overrideMimeType("text/xml");
    var self = this;
    this.request.addEventListener("load", function loadEventListener(event) { self.onLoad() }, false);
    this.request.addEventListener("error", function errorEventListener(event) { self.onError() }, false);
    this.request.send(null);
  }
  catch (e) {
    ERROR("Failed to request update manifest", e);
  }
}

UpdateParser.prototype = {
  id: null,
  updateKey: null,
  observer: null,
  request: null,
  timer: null,
  url: null,

  /**
   * Called when the manifest has been successfully loaded.
   */
  onLoad: function UP_onLoad() {
    this.timer.cancel();
    this.timer = null;
    let request = this.request;
    this.request = null;

    let requireBuiltIn = true;
    try {
      requireBuiltIn = Services.prefs.getBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS);
    }
    catch (e) {
    }

    try {
      CertUtils.checkCert(request.channel, !requireBuiltIn);
    }
    catch (e) {
      this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
      return;
    }

    if (!Components.isSuccessCode(request.status)) {
      WARN("Request failed: " + this.url + " - " + request.status);
      this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
      return;
    }

    let channel = request.channel;
    if (channel instanceof Ci.nsIHttpChannel && !channel.requestSucceeded) {
      WARN("Request failed: " + this.url + " - " + channel.responseStatus +
           ": " + channel.responseStatusText);
      this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
      return;
    }

    let xml = request.responseXML;
    if (!xml || xml.documentElement.namespaceURI == XMLURI_PARSE_ERROR) {
      WARN("Update manifest was not valid XML");
      this.notifyError(AddonUpdateChecker.ERROR_PARSE_ERROR);
      return;
    }

    // We currently only know about RDF update manifests
    if (xml.documentElement.namespaceURI == PREFIX_NS_RDF) {
      let results = null;

      try {
        results = parseRDFManifest(this.id, this.updateKey, request);
      }
      catch (e) {
        WARN(e);
        this.notifyError(AddonUpdateChecker.ERROR_PARSE_ERROR);
        return;
      }
      if ("onUpdateCheckComplete" in this.observer) {
        try {
          this.observer.onUpdateCheckComplete(results);
        }
        catch (e) {
          WARN("onUpdateCheckComplete notification failed", e);
        }
      }
      return;
    }

    WARN("Update manifest had an unrecognised namespace: " + xml.documentElement.namespaceURI);
    this.notifyError(AddonUpdateChecker.ERROR_UNKNOWN_FORMAT);
  },

  /**
   * Called when the manifest failed to load.
   */
  onError: function UP_onError() {
    this.timer.cancel();
    this.timer = null;

    if (!Components.isSuccessCode(this.request.status)) {
      WARN("Request failed: " + this.url + " - " + this.request.status);
    }
    else if (this.request.channel instanceof Ci.nsIHttpChannel) {
      try {
        if (this.request.channel.requestSucceeded) {
          WARN("Request failed: " + this.url + " - " +
               this.request.channel.responseStatus + ": " +
               this.request.channel.responseStatusText);
        }
      }
      catch (e) {
        WARN("HTTP Request failed for an unknown reason");
      }
    }
    else {
      WARN("Request failed for an unknown reason");
    }

    this.request = null;

    this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
  },

  /**
   * Helper method to notify the observer that an error occured.
   */
  notifyError: function UP_notifyError(aStatus) {
    if ("onUpdateCheckError" in this.observer) {
      try {
        this.observer.onUpdateCheckError(aStatus);
      }
      catch (e) {
        WARN("onUpdateCheckError notification failed", e);
      }
    }
  },

  /**
   * Called when the request has timed out and should be canceled.
   */
  notify: function UP_notify(aTimer) {
    this.timer = null;
    this.request.abort();
    this.request = null;

    WARN("Request timed out");

    this.notifyError(AddonUpdateChecker.ERROR_TIMEOUT);
  },

  /**
   * Called to cancel an in-progress update check.
   */
  cancel: function UP_cancel() {
    this.timer.cancel();
    this.timer = null;
    this.request.abort();
    this.request = null;
    this.notifyError(AddonUpdateChecker.ERROR_CANCELLED);
  }
};

/**
 * Tests if an update matches a version of the application or platform
 *
 * @param  aUpdate
 *         The available update
 * @param  aAppVersion
 *         The application version to use
 * @param  aPlatformVersion
 *         The platform version to use
 * @param  aIgnoreMaxVersion
 *         Ignore maxVersion when testing if an update matches. Optional.
 * @param  aIgnoreStrictCompat
 *         Ignore strictCompatibility when testing if an update matches. Optional.
 * @param  aCompatOverrides
 *         AddonCompatibilityOverride objects to match against. Optional.
 * @return true if the update is compatible with the application/platform
 */
function matchesVersions(aUpdate, aAppVersion, aPlatformVersion,
                         aIgnoreMaxVersion, aIgnoreStrictCompat,
                         aCompatOverrides) {
  if (aCompatOverrides) {
    let override = AddonRepository.findMatchingCompatOverride(aUpdate.version,
                                                              aCompatOverrides,
                                                              aAppVersion,
                                                              aPlatformVersion);
    if (override && override.type == "incompatible")
      return false;
  }

  if (aUpdate.strictCompatibility && !aIgnoreStrictCompat)
    aIgnoreMaxVersion = false;

  let result = false;
  for (let app of aUpdate.targetApplications) {
    if (app.id == Services.appinfo.ID) {
      return (Services.vc.compare(aAppVersion, app.minVersion) >= 0) &&
             (aIgnoreMaxVersion || (Services.vc.compare(aAppVersion, app.maxVersion) <= 0));
    }
    if (app.id == TOOLKIT_ID) {
      result = (Services.vc.compare(aPlatformVersion, app.minVersion) >= 0) &&
               (aIgnoreMaxVersion || (Services.vc.compare(aPlatformVersion, app.maxVersion) <= 0));
    }
  }
  return result;
}

this.AddonUpdateChecker = {
  // These must be kept in sync with AddonManager
  // The update check timed out
  ERROR_TIMEOUT: -1,
  // There was an error while downloading the update information.
  ERROR_DOWNLOAD_ERROR: -2,
  // The update information was malformed in some way.
  ERROR_PARSE_ERROR: -3,
  // The update information was not in any known format.
  ERROR_UNKNOWN_FORMAT: -4,
  // The update information was not correctly signed or there was an SSL error.
  ERROR_SECURITY_ERROR: -5,
  // The update was cancelled
  ERROR_CANCELLED: -6,

  /**
   * Retrieves the best matching compatibility update for the application from
   * a list of available update objects.
   *
   * @param  aUpdates
   *         An array of update objects
   * @param  aVersion
   *         The version of the add-on to get new compatibility information for
   * @param  aIgnoreCompatibility
   *         An optional parameter to get the first compatibility update that
   *         is compatible with any version of the application or toolkit
   * @param  aAppVersion
   *         The version of the application or null to use the current version
   * @param  aPlatformVersion
   *         The version of the platform or null to use the current version
   * @param  aIgnoreMaxVersion
   *         Ignore maxVersion when testing if an update matches. Optional.
   * @param  aIgnoreStrictCompat
   *         Ignore strictCompatibility when testing if an update matches. Optional.
   * @return an update object if one matches or null if not
   */
  getCompatibilityUpdate: function AUC_getCompatibilityUpdate(aUpdates, aVersion,
                                                              aIgnoreCompatibility,
                                                              aAppVersion,
                                                              aPlatformVersion,
                                                              aIgnoreMaxVersion,
                                                              aIgnoreStrictCompat) {
    if (!aAppVersion)
      aAppVersion = Services.appinfo.version;
    if (!aPlatformVersion)
      aPlatformVersion = Services.appinfo.platformVersion;

    for (let update of aUpdates) {
      if (Services.vc.compare(update.version, aVersion) == 0) {
        if (aIgnoreCompatibility) {
          for (let targetApp of update.targetApplications) {
            let id = targetApp.id;
            if (id == Services.appinfo.ID || id == TOOLKIT_ID)
              return update;
          }
        }
        else if (matchesVersions(update, aAppVersion, aPlatformVersion,
                                 aIgnoreMaxVersion, aIgnoreStrictCompat)) {
          return update;
        }
      }
    }
    return null;
  },

  /**
   * Returns the newest available update from a list of update objects.
   *
   * @param  aUpdates
   *         An array of update objects
   * @param  aAppVersion
   *         The version of the application or null to use the current version
   * @param  aPlatformVersion
   *         The version of the platform or null to use the current version
   * @param  aIgnoreMaxVersion
   *         When determining compatible updates, ignore maxVersion. Optional.
   * @param  aIgnoreStrictCompat
   *         When determining compatible updates, ignore strictCompatibility. Optional.
   * @param  aCompatOverrides
   *         Array of AddonCompatibilityOverride to take into account. Optional.
   * @return an update object if one matches or null if not
   */
  getNewestCompatibleUpdate: function AUC_getNewestCompatibleUpdate(aUpdates,
                                                                    aAppVersion,
                                                                    aPlatformVersion,
                                                                    aIgnoreMaxVersion,
                                                                    aIgnoreStrictCompat,
                                                                    aCompatOverrides) {
    if (!aAppVersion)
      aAppVersion = Services.appinfo.version;
    if (!aPlatformVersion)
      aPlatformVersion = Services.appinfo.platformVersion;

    let blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                    getService(Ci.nsIBlocklistService);

    let newest = null;
    for (let update of aUpdates) {
      if (!update.updateURL)
        continue;
      let state = blocklist.getAddonBlocklistState(update.id, update.version,
                                                   aAppVersion, aPlatformVersion);
      if (state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED)
        continue;
      if ((newest == null || (Services.vc.compare(newest.version, update.version) < 0)) &&
          matchesVersions(update, aAppVersion, aPlatformVersion,
                          aIgnoreMaxVersion, aIgnoreStrictCompat,
                          aCompatOverrides)) {
        newest = update;
      }
    }
    return newest;
  },

  /**
   * Starts an update check.
   *
   * @param  aId
   *         The ID of the add-on being checked for updates
   * @param  aUpdateKey
   *         An optional update key for the add-on
   * @param  aUrl
   *         The URL of the add-on's update manifest
   * @param  aObserver
   *         An observer to notify of results
   * @return UpdateParser so that the caller can use UpdateParser.cancel() to shut
   *         down in-progress update requests
   */
  checkForUpdates: function AUC_checkForUpdates(aId, aUpdateKey, aUrl,
                                                aObserver) {
    return new UpdateParser(aId, aUpdateKey, aUrl, aObserver);
  }
};
