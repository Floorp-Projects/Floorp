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
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
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

/**
 * The AddonUpdateChecker is responsible for retrieving the update information
 * from an add-on's remote update manifest.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

var EXPORTED_SYMBOLS = [ "AddonUpdateChecker" ];

const TIMEOUT               = 2 * 60 * 1000;
const PREFIX_NS_RDF         = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const PREFIX_NS_EM          = "http://www.mozilla.org/2004/em-rdf#";
const PREFIX_ITEM           = "urn:mozilla:item:";
const PREFIX_EXTENSION      = "urn:mozilla:extension:";
const PREFIX_THEME          = "urn:mozilla:theme:";
const TOOLKIT_ID            = "toolkit@mozilla.org"
const XMLURI_PARSE_ERROR    = "http://www.mozilla.org/newlayout/xml/parsererror.xml"

Components.utils.import("resource://gre/modules/Services.jsm");
// shared code for suppressing bad cert dialogs
Components.utils.import("resource://gre/modules/CertUtils.jsm");

var gRDF = Cc["@mozilla.org/rdf/rdf-service;1"].
           getService(Ci.nsIRDFService);

/**
 * Logs a debug message.
 *
 * @param   str
 *          The string to log
 */
function LOG(str) {
  dump("*** addons.updates: " + str + "\n");
}

/**
 * Logs a warning message
 *
 * @param   str
 *          The string to log
 */
function WARN(str) {
  LOG(str);
}

/**
 * Logs an error message
 *
 * @param   str
 *          The string to log
 */
function ERROR(str) {
  LOG(str);
}

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
   * @param   string
   *          The string to be escaped
   * @return  a string with all characters invalid in XML character data
   *          converted to entity references.
   */
  escapeEntities: function RDFS_escapeEntities(string) {
    string = string.replace(/&/g, "&amp;");
    string = string.replace(/</g, "&lt;");
    string = string.replace(/>/g, "&gt;");
    return string.replace(/"/g, "&quot;");
  },

  /**
   * Serializes all the elements of an RDF container.
   *
   * @param   ds
   *          The RDF datasource
   * @param   container
   *          The RDF container to output the child elements of
   * @param   indent
   *          The current level of indent for pretty-printing
   * @return  a string containing the serialized elements.
   */
  serializeContainerItems: function RDFS_serializeContainerItems(ds, container, indent) {
    var result = "";
    var items = container.GetElements();
    while (items.hasMoreElements()) {
      var item = items.getNext().QueryInterface(Ci.nsIRDFResource);
      result += indent + "<RDF:li>\n"
      result += this.serializeResource(ds, item, indent + this.INDENT);
      result += indent + "</RDF:li>\n"
    }
    return result;
  },

  /**
   * Serializes all em:* (see EM_NS) properties of an RDF resource except for
   * the em:signature property. As this serialization is to be compared against
   * the manifest signature it cannot contain the em:signature property itself.
   *
   * @param   ds
   *          The RDF datasource
   * @param   resource
   *          The RDF resource that contains the properties to serialize
   * @param   indent
   *          The current level of indent for pretty-printing
   * @return  a string containing the serialized properties.
   * @throws  if the resource contains a property that cannot be serialized
   */
  serializeResourceProperties: function RDFS_serializeResourceProperties(ds, resource, indent) {
    var result = "";
    var items = [];
    var arcs = ds.ArcLabelsOut(resource);
    while (arcs.hasMoreElements()) {
      var arc = arcs.getNext().QueryInterface(Ci.nsIRDFResource);
      if (arc.ValueUTF8.substring(0, PREFIX_NS_EM.length) != PREFIX_NS_EM)
        continue;
      var prop = arc.ValueUTF8.substring(PREFIX_NS_EM.length);
      if (prop == "signature")
        continue;

      var targets = ds.GetTargets(resource, arc, true);
      while (targets.hasMoreElements()) {
        var target = targets.getNext();
        if (target instanceof Ci.nsIRDFResource) {
          var item = indent + "<em:" + prop + ">\n";
          item += this.serializeResource(ds, target, indent + this.INDENT);
          item += indent + "</em:" + prop + ">\n";
          items.push(item);
        }
        else if (target instanceof Ci.nsIRDFLiteral) {
          items.push(indent + "<em:" + prop + ">" + this.escapeEntities(target.Value) + "</em:" + prop + ">\n");
        }
        else if (target instanceof Ci.nsIRDFInt) {
          items.push(indent + "<em:" + prop + " NC:parseType=\"Integer\">" + target.Value + "</em:" + prop + ">\n");
        }
        else {
          throw new Error("Cannot serialize unknown literal type");
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
   * @param   ds
   *          The RDF datasource
   * @param   resource
   *          The RDF resource to serialize
   * @param   indent
   *          The current level of indent for pretty-printing. If undefined no
   *          indent will be added
   * @return  a string containing the serialized resource.
   * @throws  if the RDF data contains multiple references to the same resource.
   */
  serializeResource: function RDFS_serializeResource(ds, resource, indent) {
    if (this.resources.indexOf(resource) != -1 ) {
      // We cannot output multiple references to the same resource.
      throw new Error("Cannot serialize multiple references to " + resource.Value);
    }
    if (indent === undefined)
      indent = "";

    this.resources.push(resource);
    var container = null;
    var type = "Description";
    if (this.cUtils.IsSeq(ds, resource)) {
      type = "Seq";
      container = this.cUtils.MakeSeq(ds, resource);
    }
    else if (this.cUtils.IsAlt(ds, resource)) {
      type = "Alt";
      container = this.cUtils.MakeAlt(ds, resource);
    }
    else if (this.cUtils.IsBag(ds, resource)) {
      type = "Bag";
      container = this.cUtils.MakeBag(ds, resource);
    }

    var result = indent + "<RDF:" + type;
    if (!gRDF.IsAnonymousResource(resource))
      result += " about=\"" + this.escapeEntities(resource.ValueUTF8) + "\"";
    result += ">\n";

    if (container)
      result += this.serializeContainerItems(ds, container, indent + this.INDENT);

    result += this.serializeResourceProperties(ds, resource, indent + this.INDENT);

    result += indent + "</RDF:" + type + ">\n";
    return result;
  }
}

/**
 * Parses an RDF style update manifest into an array of update objects.
 *
 * @param   id
 *          The ID of the add-on being checked for updates
 * @param   type
 *          The type of the add-on being checked for updates
 * @param   updateKey
 *          An optional update key for the add-on
 * @param   The XMLHttpRequest that has retrieved the update manifest
 * @return  an array of update objects
 * @throws  if the update manifest is invalid in any way
 */
function parseRDFManifest(id, type, updateKey, request) {
  function EM_R(prop) {
    return gRDF.GetResource(PREFIX_NS_EM + prop);
  }

  function getValue(literal) {
    if (literal instanceof Ci.nsIRDFLiteral)
      return literal.Value;
    if (literal instanceof Ci.nsIRDFResource)
      return literal.Value;
    if (literal instanceof Ci.nsIRDFInt)
      return literal.Value;
    return null;
  }

  function getProperty(ds, source, property) {
    return getValue(ds.GetTarget(source, EM_R(property), true));
  }

  function getRequiredProperty(ds, source, property) {
    let value = getProperty(ds, source, property);
    if (!value)
      throw new Error("Missing required property " + property);
    return value;
  }

  let rdfParser = Cc["@mozilla.org/rdf/xml-parser;1"].
                  createInstance(Ci.nsIRDFXMLParser);
  let ds = Cc["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].
           createInstance(Ci.nsIRDFDataSource);
  rdfParser.parseString(ds, request.channel.URI, request.responseText);

  switch (type) {
  case "extension":
    var item = PREFIX_EXTENSION + id;
    break;
  case "theme":
    item = PREFIX_THEME + id;
    break;
  default:
    item = PREFIX_ITEM + id;
    break;
  }

  let extensionRes  = gRDF.GetResource(item);

  // If we have an update key then the update manifest must be signed
  if (updateKey) {
    let signature = getProperty(ds, extensionRes, "signature");
    if (!signature)
      throw new Error("Update manifest for " + id + " does not contain a required signature");
    let serializer = new RDFSerializer();
    let updateString = null;

    try {
      updateString = serializer.serializeResource(ds, extensionRes);
    }
    catch (e) {
      throw new Error("Failed to generate signed string for " + id + ". Serializer threw " + e);
    }

    let result = false;

    try {
      let verifier = Cc["@mozilla.org/security/datasignatureverifier;1"].
                     getService(Ci.nsIDataSignatureVerifier);
      result = verifier.verifyData(updateString, signature, updateKey);
    }
    catch (e) {
      throw new Error("The signature or updateKey for " + id + " is malformed");
    }

    if (!result)
      throw new Error("The signature for " + id + " was not created by the add-on's updateKey");
  }

  let updates = ds.GetTarget(extensionRes, EM_R("updates"), true);

  if (!updates || !(updates instanceof Ci.nsIRDFResource))
    throw new Error("Missing updates property for " + extensionRes.Value);

  let cu = Cc["@mozilla.org/rdf/container-utils;1"].
           getService(Ci.nsIRDFContainerUtils);
  if (!cu.IsContainer(ds, updates))
    throw new Error("Updates property was not an RDF container");

  let checkSecurity = true;

  try {
    checkSecurity = Services.prefs.getBoolPref("extensions.checkUpdateSecurity");
  }
  catch (e) {
  }

  let results = [];
  let ctr = Cc["@mozilla.org/rdf/container;1"].
            createInstance(Ci.nsIRDFContainer);
  ctr.Init(ds, updates);
  let items = ctr.GetElements();
  while (items.hasMoreElements()) {
    let item = items.getNext().QueryInterface(Ci.nsIRDFResource);
    let version = getRequiredProperty(ds, item, "version");
    LOG("Found an update entry for " + id + " version " + version);

    let targetApps = ds.GetTargets(item, EM_R("targetApplication"), true);
    while (targetApps.hasMoreElements()) {
      let targetApp = targetApps.getNext().QueryInterface(Ci.nsIRDFResource);

      let result = {
        version: version,
        updateURL: getProperty(ds, targetApp, "updateLink"),
        updateHash: getProperty(ds, targetApp, "updateHash"),
        updateInfoURL: getProperty(ds, targetApp, "updateInfoURL"),
        targetApplications: [{
          id: getRequiredProperty(ds, targetApp, "id"),
          minVersion: getRequiredProperty(ds, targetApp, "minVersion"),
          maxVersion: getRequiredProperty(ds, targetApp, "maxVersion"),
        }]
      };

      if (result.updateURL && checkSecurity &&
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
 * @param   id
 *          The ID of the add-on being checked for updates
 * @param   type
 *          The type of add-on being checked for updates
 * @param   updateKey
 *          An optional update key for the add-on
 * @param   url
 *          The URL of the update manifest
 * @param   observer
 *          An observer to pass results to
 */
function UpdateParser(id, type, updateKey, url, observer) {
  this.id = id;
  this.type = type;
  this.updateKey = updateKey;
  this.observer = observer;

  this.timer = Cc["@mozilla.org/timer;1"].
               createInstance(Ci.nsITimer);
  this.timer.initWithCallback(this, TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);

  LOG("Requesting " + url);
  this.request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                 createInstance(Ci.nsIXMLHttpRequest);
  this.request.open("GET", url, true);
  this.request.channel.notificationCallbacks = new BadCertHandler();
  this.request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  this.request.overrideMimeType("text/xml");
  var self = this;
  this.request.onload = function(event) { self.onLoad() };
  this.request.onerror = function(event) { self.onError() };
  this.request.send(null);
}

UpdateParser.prototype = {
  id: null,
  type: null,
  updateKey: null,
  observer: null,
  request: null,
  timer: null,

  /**
   * Called when the manifest has been successfully loaded.
   */
  onLoad: function UP_onLoad() {
    this.timer.cancel();
    this.timer = null;
    let request = this.request;
    this.request = null;

    try {
      checkCert(request.channel);
    }
    catch (e) {
      this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
      return;
    }

    if (!Components.isSuccessCode(request.status)) {
      WARN("Request failed: " + request.status);
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
        results = parseRDFManifest(this.id, this.type, this.updateKey, request);
      }
      catch (e) {
        WARN(e);
        this.notifyError(AddonUpdateChecker.ERROR_PARSE_ERROR);
        return;
      }
      if ("onUpdateCheckComplete" in this.observer)
        this.observer.onUpdateCheckComplete(results);
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

    WARN("Request failed: " + this.request.status);
    this.request = null;

    this.notifyError(AddonUpdateChecker.ERROR_DOWNLOAD_ERROR);
  },

  /**
   * Helper method to notify the observer that an error occured.
   */
  notifyError: function UP_notifyError(status) {
    if ("onUpdateCheckError" in this.observer)
      this.observer.onUpdateCheckError(status);
  },

  /**
   * Called when the request has timed out and should be canceled.
   */
  notify: function UP_notify(timer) {
    this.timer = null;
    this.request.abort();
    this.request = null;

    WARN("Request timed out");

    this.notifyError(AddonUpdateChecker.ERROR_TIMEOUT);
  }
};

/**
 * Tests if an update matches a version of the application or platform
 *
 * @param   update
 *          The available update
 * @param   appVersion
 *          The application version to use
 * @param   platformVersion
 *          The platform version to use
 * @return  true if the update is compatible with the application/platform
 */
function matchesVersions(update, appVersion, platformVersion) {
  let result = false;
  for (let i = 0; i < update.targetApplications.length; i++) {
    let app = update.targetApplications[i];
    if (app.id == Services.appinfo.ID) {
      return (Services.vc.compare(appVersion, app.minVersion) >= 0) &&
             (Services.vc.compare(appVersion, app.maxVersion) <= 0);
    }
    if (app.id == TOOLKIT_ID) {
      result = (Services.vc.compare(platformVersion, app.minVersion) >= 0) &&
               (Services.vc.compare(platformVersion, app.maxVersion) <= 0);
    }
  }
  return result;
}

var AddonUpdateChecker = {
  ERROR_TIMEOUT: -1,
  ERROR_DOWNLOAD_ERROR: -2,
  ERROR_PARSE_ERROR: -3,
  ERROR_UNKNOWN_FORMAT: -4,
  ERROR_SECURITY_ERROR: -5,

  /**
   * Retrieves the best matching compatibility update for the application from
   * a list of available update objects.
   *
   * @param   updates
   *          An array of update objects
   * @param   version
   *          The version of the add-on to get new compatibility information for
   * @param   ignoreCompatibility
   *          An optional parameter to get the first compatibility update that
   *          is compatible with any version of the application or toolkit
   * @param   appVersion
   *          The version of the application or null to use the current version
   * @param   platformVersion
   *          The version of the platform or null to use the current version
   * @return  an update object if one matches or null if not
   */
  getCompatibilityUpdate: function AUC_getCompatibilityUpdate(updates, version,
                                                              ignoreCompatibility,
                                                              appVersion,
                                                              platformVersion) {
    if (!appVersion)
      appVersion = Services.appinfo.version;
    if (!platformVersion)
      platformVersion = Services.appinfo.platformVersion;

    for (let i = 0; i < updates.length; i++) {
      if (Services.vc.compare(updates[i].version, version) == 0) {
        if (ignoreCompatibility) {
          for (let j = 0; j < updates[i].targetApplications.length; j++) {
            let id = updates[i].targetApplications[j].id;
            if (id == Services.appinfo.ID || id == TOOLKIT_ID)
              return updates[i];
          }
        }
        else if (matchesVersions(updates[i], appVersion, platformVersion)) {
          return updates[i];
        }
      }
    }
    return null;
  },

  /**
   * Returns the newest available update from a list of update objects.
   *
   * @param   updates
   *          An array of update objects
   * @param   appVersion
   *          The version of the application or null to use the current version
   * @param   platformVersion
   *          The version of the platform or null to use the current version
   * @return  an update object if one matches or null if not
   */
  getNewestCompatibleUpdate: function AUC_getNewestCompatibleUpdate(updates,
                                                                    appVersion,
                                                                    platformVersion) {
    if (!appVersion)
      appVersion = Services.appinfo.version;
    if (!platformVersion)
      platformVersion = Services.appinfo.platformVersion;

    let newest = null;
    for (let i = 0; i < updates.length; i++) {
      if (!updates[i].updateURL)
        continue;
      if ((newest == null || (Services.vc.compare(newest.version, updates[i].version) < 0)) &&
          matchesVersions(updates[i], appVersion, platformVersion))
        newest = updates[i];
    }
    return newest;
  },

  /**
   * Starts an update check.
   *
   * @param   id
   *          The ID of the add-on being checked for updates
   * @param   type
   *          The type of add-on being checked for updates
   * @param   updateKey
   *          An optional update key for the add-on
   * @param   url
   *          The URL of the add-on's update manifest
   * @param   observer
   *          An observer to notify of results
   */
  checkForUpdates: function AUC_checkForUpdates(id, type, updateKey, url,
                                                observer) {
    new UpdateParser(id, type, updateKey, url, observer);
  }
};
