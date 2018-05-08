/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["UpdateRDFConverter"];

const PREFIX_NS_EM          = "http://www.mozilla.org/2004/em-rdf#";
const PREFIX_ITEM           = "urn:mozilla:item:";
const PREFIX_EXTENSION      = "urn:mozilla:extension:";
const PREFIX_THEME          = "urn:mozilla:theme:";

const TOOLKIT_ID            = "toolkit@mozilla.org";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  gRDF: ["@mozilla.org/rdf/rdf-service;1", "nsIRDFService"],
  containerUtils: ["@mozilla.org/rdf/container-utils;1", "nsIRDFContainerUtils"],
});

const RDFContainer = Components.Constructor(
  "@mozilla.org/rdf/container;1", "nsIRDFContainer", "Init");
const RDFDataSource = Components.Constructor(
  "@mozilla.org/rdf/datasource;1?name=in-memory-datasource", "nsIRDFDataSource");
const RDFParser = Components.Constructor(
  "@mozilla.org/rdf/xml-parser;1", "nsIRDFXMLParser");

/**
 * Parses an RDF style update manifest into a JSON-style update
 * manifest.
 *
 * @param {XMLHttpRequest> aRequest
 *         The XMLHttpRequest that has retrieved the update manifest
 * @returns {object} a JSON update manifest.
 */
function parseRDFManifest(aRequest) {
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

  function getBooleanProperty(aDs, aSource, aProperty) {
    let propValue = aDs.GetTarget(aSource, EM_R(aProperty), true);
    if (!propValue)
      return undefined;
    return getValue(propValue) == "true";
  }

  let rdfParser = new RDFParser();
  let ds = new RDFDataSource();
  rdfParser.parseString(ds, aRequest.channel.URI, aRequest.responseText);

  let addons = {};
  let result = {addons};

  for (let addonRes of XPCOMUtils.IterSimpleEnumerator(ds.GetAllResources(), Ci.nsIRDFResource)) {
    let value = addonRes.ValueUTF8;
    let id;
    for (let prefix of [PREFIX_EXTENSION, PREFIX_THEME, PREFIX_ITEM]) {
      if (value.startsWith(prefix)) {
        id = value.substr(prefix.length);
        break;
      }
    }
    if (!id) {
      continue;
    }

    let addon = {};
    addons[id] = addon;

    let updatesTarget = ds.GetTarget(addonRes, EM_R("updates"), true);
    if (!(updatesTarget instanceof Ci.nsIRDFResource) ||
        !containerUtils.IsContainer(ds, updatesTarget)) {
      continue;
    }

    let updates = [];
    addon.updates = updates;

    let ctr = new RDFContainer(ds, updatesTarget);
    for (let item of XPCOMUtils.IterSimpleEnumerator(ctr.GetElements(),
                                                     Ci.nsIRDFResource)) {
      let version = getProperty(ds, item, "version");

      let targetApps = ds.GetTargets(item, EM_R("targetApplication"), true);
      for (let targetApp of XPCOMUtils.IterSimpleEnumerator(targetApps,
                                                            Ci.nsIRDFResource)) {
        let appEntry = {};

        let minVersion = getProperty(ds, targetApp, "minVersion");
        if (minVersion) {
          appEntry.strict_min_version = minVersion;
        }

        let maxVersion = getProperty(ds, targetApp, "maxVersion");
        if (maxVersion) {
          if (getBooleanProperty(ds, targetApp, "strictCompatibility")) {
            appEntry.strict_max_version = maxVersion;
          } else {
            appEntry.advisory_max_version = maxVersion;
          }
        }

        let appId = getProperty(ds, targetApp, "id");
        if (!appId) {
          continue;
        }
        if (appId === TOOLKIT_ID || appId === Services.appinfo.ID) {
          appId = "gecko";
        }

        let update = {
          applications: {[appId]: appEntry},
        };
        if (version) {
          update.version = version;
        }
        let updateLink = getProperty(ds, targetApp, "updateLink");
        if (updateLink) {
          update.update_link = updateLink;
        }
        let updateInfoURL = getProperty(ds, targetApp, "updateInfoURL");
        if (updateInfoURL) {
          update.update_info_url = updateInfoURL;
        }
        let updateHash = getProperty(ds, targetApp, "updateHash");
        if (updateHash) {
          update.update_hash = updateHash;
        }

        updates.push(update);
      }
    }
  }

  return result;
}

var UpdateRDFConverter = {
  convertToJSON(request) {
    return parseRDFManifest(request);
  },
};
