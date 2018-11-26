 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["UpdateRDFConverter"];

ChromeUtils.defineModuleGetter(this, "RDFDataSource",
                               "resource://gre/modules/addons/RDFDataSource.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const PREFIX_ITEM           = "urn:mozilla:item:";
const PREFIX_EXTENSION      = "urn:mozilla:extension:";
const PREFIX_THEME          = "urn:mozilla:theme:";

const TOOLKIT_ID            = "toolkit@mozilla.org";

function EM_R(aProperty) {
  return `http://www.mozilla.org/2004/em-rdf#${aProperty}`;
}

function getValue(literal) {
  return literal && literal.getValue();
}

function getProperty(resource, property) {
  return getValue(resource.getProperty(EM_R(property)));
}

function getBoolProperty(resource, property) {
  return getValue(resource.getProperty(EM_R(property))) == "true";
}

class Manifest {
  constructor(ds) {
    this.ds = ds;
  }

  static loadFromString(text) {
    return new this(RDFDataSource.loadFromString(text));
  }

  static loadFromBuffer(buffer) {
    return new this(RDFDataSource.loadFromBuffer(buffer));
  }

  static async loadFromFile(uri) {
    return new this(await RDFDataSource.loadFromFile(uri));
  }
}

class UpdateRDF extends Manifest {
  decode() {
    let addons = {};
    let result = {addons};

    for (let resource of this.ds.getAllResources()) {
      let id;
      let uri = resource.getURI();
      for (let prefix of [PREFIX_EXTENSION, PREFIX_THEME, PREFIX_ITEM]) {
        if (uri.startsWith(prefix)) {
          id = uri.substr(prefix.length);
          break;
        }
      }
      if (!id) {
        continue;
      }

      let addon = {};
      addons[id] = addon;

      let updatesProp = resource.getProperty(EM_R("updates"));
      if (!updatesProp || !updatesProp.getChildren) {
        continue;
      }

      let updates = [];
      addon.updates = updates;

      for (let item of updatesProp.getChildren()) {
        let version = getProperty(item, "version");

        for (let targetApp of item.getObjects(EM_R("targetApplication"))) {
          let appEntry = {};

          let minVersion = getProperty(targetApp, "minVersion");
          if (minVersion) {
            appEntry.strict_min_version = minVersion;
          }

          let maxVersion = getProperty(targetApp, "maxVersion");
          if (maxVersion) {
            if (getBoolProperty(targetApp, "strictCompatibility")) {
              appEntry.strict_max_version = maxVersion;
            } else {
              appEntry.advisory_max_version = maxVersion;
            }
          }

          let appId = getProperty(targetApp, "id");
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
          let updateLink = getProperty(targetApp, "updateLink");
          if (updateLink) {
            update.update_link = updateLink;
          }
          let updateInfoURL = getProperty(targetApp, "updateInfoURL");
          if (updateInfoURL) {
            update.update_info_url = updateInfoURL;
          }
          let updateHash = getProperty(targetApp, "updateHash");
          if (updateHash) {
            update.update_hash = updateHash;
          }

          updates.push(update);
        }
      }
    }

    return result;
  }
}

var UpdateRDFConverter = {
  convertToJSON(request) {
    return UpdateRDF.loadFromString(request.responseText).decode();
  },
};
