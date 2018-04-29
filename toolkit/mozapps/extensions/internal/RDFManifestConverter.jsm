 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["InstallRDF", "UpdateRDFConverter"];

ChromeUtils.defineModuleGetter(this, "RDFDataSource",
                               "resource://gre/modules/addons/RDFDataSource.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const PREFIX_ITEM           = "urn:mozilla:item:";
const PREFIX_EXTENSION      = "urn:mozilla:extension:";
const PREFIX_THEME          = "urn:mozilla:theme:";

const TOOLKIT_ID            = "toolkit@mozilla.org";

const RDFURI_INSTALL_MANIFEST_ROOT = "urn:mozilla:install-manifest";

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

class InstallRDF extends Manifest {
  _readProps(source, obj, props) {
    for (let prop of props) {
      let val = getProperty(source, prop);
      if (val != null) {
        obj[prop] = val;
      }
    }
  }

  _readArrayProp(source, obj, prop, target, decode = getValue) {
    let result = Array.from(source.getObjects(EM_R(prop)),
                            target => decode(target));
    if (result.length) {
      obj[target] = result;
    }
  }

  _readArrayProps(source, obj, props, decode = getValue) {
    for (let [prop, target] of Object.entries(props)) {
      this._readArrayProp(source, obj, prop, target, decode);
    }
  }

  _readLocaleStrings(source, obj) {
    this._readProps(source, obj, ["name", "description", "creator", "homepageURL"]);
    this._readArrayProps(source, obj, {
      locale: "locales",
      developer: "developers",
      translator: "translators",
      contributor: "contributors",
    });
  }

  decode() {
    let root = this.ds.getResource(RDFURI_INSTALL_MANIFEST_ROOT);
    let result = {};

    let props = ["id", "version", "type", "updateURL", "optionsURL",
                 "optionsType", "aboutURL", "iconURL", "icon64URL",
                 "bootstrap", "unpack", "strictCompatibility",
                 "hasEmbeddedWebExtension"];
    this._readProps(root, result, props);

    let decodeTargetApplication = source => {
      let app = {};
      this._readProps(source, app, ["id", "minVersion", "maxVersion"]);
      return app;
    };

    let decodeLocale = source => {
      let localized = {};
      this._readLocaleStrings(source, localized);
      return localized;
    };

    this._readLocaleStrings(root, result);

    this._readArrayProps(root, result, {"targetPlatform": "targetPlatforms"});
    this._readArrayProps(root, result, {"targetApplication": "targetApplications"},
                         decodeTargetApplication);
    this._readArrayProps(root, result, {"localized": "localized"},
                         decodeLocale);
    this._readArrayProps(root, result, {"dependency": "dependencies"},
                         source => getProperty(source, "id"));

    return result;
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
