/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported ProductAddonChecker */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const LOCAL_EME_SOURCES = [{
  "id": "gmp-eme-adobe",
  "src": "chrome://global/content/gmp-sources/eme-adobe.json"
}, {
  "id": "gmp-gmpopenh264",
  "src": "chrome://global/content/gmp-sources/openh264.json"
}, {
  "id": "gmp-widevinecdm",
  "src": "chrome://global/content/gmp-sources/widevinecdm.json"
}];

this.EXPORTED_SYMBOLS = [ "ProductAddonChecker" ];

Cu.importGlobalProperties(["XMLHttpRequest"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/CertUtils.jsm");
/* globals checkCert, BadCertHandler*/
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

/* globals GMPPrefs */
XPCOMUtils.defineLazyModuleGetter(this, "GMPPrefs",
                                  "resource://gre/modules/GMPUtils.jsm");

/* globals OS */

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");

var logger = Log.repository.getLogger("addons.productaddons");

// This exists so that tests can override the XHR behaviour for downloading
// the addon update XML file.
var CreateXHR = function() {
  return Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
    createInstance(Ci.nsISupports);
}

/**
 * Number of milliseconds after which we need to cancel `downloadXML`.
 *
 * Bug 1087674 suggests that the XHR we use in `downloadXML` may
 * never terminate in presence of network nuisances (e.g. strange
 * antivirus behavior). This timeout is a defensive measure to ensure
 * that we fail cleanly in such case.
 */
const TIMEOUT_DELAY_MS = 20000;
// How much of a file to read into memory at a time for hashing
const HASH_CHUNK_SIZE = 8192;

/**
 * Gets the status of an XMLHttpRequest either directly or from its underlying
 * channel.
 *
 * @param  request
 *         The XMLHttpRequest.
 * @return an integer status value.
 */
function getRequestStatus(request) {
  let status = null;
  try {
    status = request.status;
  }
  catch (e) {
  }

  if (status != null) {
    return status;
  }

  return request.channel.QueryInterface(Ci.nsIRequest).status;
}

/**
 * Downloads an XML document from a URL optionally testing the SSL certificate
 * for certain attributes.
 *
 * @param  url
 *         The url to download from.
 * @param  allowNonBuiltIn
 *         Whether to trust SSL certificates without a built-in CA issuer.
 * @param  allowedCerts
 *         The list of certificate attributes to match the SSL certificate
 *         against or null to skip checks.
 * @return a promise that resolves to the DOM document downloaded or rejects
 *         with a JS exception in case of error.
 */
function downloadXML(url, allowNonBuiltIn = false, allowedCerts = null) {
  return new Promise((resolve, reject) => {
    let request = CreateXHR();
    // This is here to let unit test code override XHR
    if (request.wrappedJSObject) {
      request = request.wrappedJSObject;
    }
    request.open("GET", url, true);
    request.channel.notificationCallbacks = new BadCertHandler(allowNonBuiltIn);
    // Prevent the request from reading from the cache.
    request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to the cache.
    request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    request.timeout = TIMEOUT_DELAY_MS;

    request.overrideMimeType("text/xml");
    // The Cache-Control header is only interpreted by proxies and the
    // final destination. It does not help if a resource is already
    // cached locally.
    request.setRequestHeader("Cache-Control", "no-cache");
    // HTTP/1.0 servers might not implement Cache-Control and
    // might only implement Pragma: no-cache
    request.setRequestHeader("Pragma", "no-cache");

    let fail = (event) => {
      let request = event.target;
      let status = getRequestStatus(request);
      let message = "Failed downloading XML, status: " + status +  ", reason: " + event.type;
      logger.warn(message);
      let ex = new Error(message);
      ex.status = status;
      reject(ex);
    };

    let success = (event) => {
      logger.info("Completed downloading document");
      let request = event.target;

      try {
        checkCert(request.channel, allowNonBuiltIn, allowedCerts);
      } catch (ex) {
        logger.error("Request failed certificate checks: " + ex);
        ex.status = getRequestStatus(request);
        reject(ex);
        return;
      }

      resolve(request.responseXML);
    };

    request.addEventListener("error", fail, false);
    request.addEventListener("abort", fail, false);
    request.addEventListener("timeout", fail, false);
    request.addEventListener("load", success, false);

    logger.info("sending request to: " + url);
    request.send(null);
  });
}

function downloadJSON(uri) {
  logger.info("fetching config from: " + uri);
  return new Promise((resolve, reject) => {
    let xmlHttp = new XMLHttpRequest({mozAnon: true});

    xmlHttp.onload = function(aResponse) {
      resolve(JSON.parse(this.responseText));
    };

    xmlHttp.onerror = function(e) {
      reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    xmlHttp.open("GET", uri);
    xmlHttp.overrideMimeType("application/json");
    xmlHttp.send();
  });
}


/**
 * Parses a list of add-ons from a DOM document.
 *
 * @param  document
 *         The DOM document to parse.
 * @return null if there is no <addons> element otherwise an object containing
 *         an array of the addons listed and a field notifying whether the
 *         fallback was used.
 */
function parseXML(document) {
  // Check that the root element is correct
  if (document.documentElement.localName != "updates") {
    throw new Error("got node name: " + document.documentElement.localName +
                    ", expected: updates");
  }

  // Check if there are any addons elements in the updates element
  let addons = document.querySelector("updates:root > addons");
  if (!addons) {
    return null;
  }

  let results = [];
  let addonList = document.querySelectorAll("updates:root > addons > addon");
  for (let addonElement of addonList) {
    let addon = {};

    for (let name of ["id", "URL", "hashFunction", "hashValue", "version", "size"]) {
      if (addonElement.hasAttribute(name)) {
        addon[name] = addonElement.getAttribute(name);
      }
    }
    addon.size = Number(addon.size) || undefined;

    results.push(addon);
  }

  return {
    usedFallback: false,
    gmpAddons: results
  };
}

/**
 * If downloading from the network fails (AUS server is down),
 * load the sources from local build configuration.
 */
function downloadLocalConfig() {

  if (!GMPPrefs.get(GMPPrefs.KEY_UPDATE_ENABLED, true)) {
    logger.info("Updates are disabled via media.gmp-manager.updateEnabled");
    return Promise.resolve({usedFallback: true, gmpAddons: []});
  }

  return Promise.all(LOCAL_EME_SOURCES.map(conf => {
    return downloadJSON(conf.src).then(addons => {

      let platforms = addons.vendors[conf.id].platforms;
      let target = Services.appinfo.OS + "_" + UpdateUtils.ABI;
      let details = null;

      while (!details) {
        if (!(target in platforms)) {
          // There was no matching platform so return false, this addon
          // will be filtered from the results below
          logger.info("no details found for: " + target);
          return false;
        }
        // Field either has the details of the binary or is an alias
        // to another build target key that does
        if (platforms[target].alias) {
          target = platforms[target].alias;
        } else {
          details = platforms[target];
        }
      }

      logger.info("found plugin: " + conf.id);
      return {
        "id": conf.id,
        "URL": details.fileUrl,
        "hashFunction": addons.hashFunction,
        "hashValue": details.hashValue,
        "version": addons.vendors[conf.id].version,
        "size": details.filesize
      };
    });
  })).then(addons => {

    // Some filters may not match this platform so
    // filter those out
    addons = addons.filter(x => x !== false);

    return {
      usedFallback: true,
      gmpAddons: addons
    };
  });
}

/**
 * Downloads file from a URL using XHR.
 *
 * @param  url
 *         The url to download from.
 * @return a promise that resolves to the path of a temporary file or rejects
 *         with a JS exception in case of error.
 */
function downloadFile(url) {
  return new Promise((resolve, reject) => {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                  createInstance(Ci.nsISupports);
    xhr.onload = function(response) {
      logger.info("downloadXHR File download. status=" + xhr.status);
      if (xhr.status != 200 && xhr.status != 206) {
        reject(Components.Exception("File download failed", xhr.status));
        return;
      }
      Task.spawn(function* () {
        let f = yield OS.File.openUnique(OS.Path.join(OS.Constants.Path.tmpDir, "tmpaddon"));
        let path = f.path;
        logger.info(`Downloaded file will be saved to ${path}`);
        yield f.file.close();
        yield OS.File.writeAtomic(path, new Uint8Array(xhr.response));
        return path;
      }).then(resolve, reject);
    };

    let fail = (event) => {
      let request = event.target;
      let status = getRequestStatus(request);
      let message = "Failed downloading via XHR, status: " + status +  ", reason: " + event.type;
      logger.warn(message);
      let ex = new Error(message);
      ex.status = status;
      reject(ex);
    };
    xhr.addEventListener("error", fail);
    xhr.addEventListener("abort", fail);

    xhr.responseType = "arraybuffer";
    try {
      xhr.open("GET", url);
      xhr.send(null);
    } catch (ex) {
      reject(ex);
    }
  });
}

/**
 * Convert a string containing binary values to hex.
 */
function binaryToHex(input) {
  let result = "";
  for (let i = 0; i < input.length; ++i) {
    let hex = input.charCodeAt(i).toString(16);
    if (hex.length == 1) {
      hex = "0" + hex;
    }
    result += hex;
  }
  return result;
}

/**
 * Calculates the hash of a file.
 *
 * @param  hashFunction
 *         The type of hash function to use, must be supported by nsICryptoHash.
 * @param  path
 *         The path of the file to hash.
 * @return a promise that resolves to hash of the file or rejects with a JS
 *         exception in case of error.
 */
var computeHash = Task.async(function*(hashFunction, path) {
  let file = yield OS.File.open(path, { existing: true, read: true });
  try {
    let hasher = Cc["@mozilla.org/security/hash;1"].
                 createInstance(Ci.nsICryptoHash);
    hasher.initWithString(hashFunction);

    let bytes;
    do {
      bytes = yield file.read(HASH_CHUNK_SIZE);
      hasher.update(bytes, bytes.length);
    } while (bytes.length == HASH_CHUNK_SIZE);

    return binaryToHex(hasher.finish(false));
  }
  finally {
    yield file.close();
  }
});

/**
 * Verifies that a downloaded file matches what was expected.
 *
 * @param  properties
 *         The properties to check, `size` and `hashFunction` with `hashValue`
 *         are supported. Any properties missing won't be checked.
 * @param  path
 *         The path of the file to check.
 * @return a promise that resolves if the file matched or rejects with a JS
 *         exception in case of error.
 */
var verifyFile = Task.async(function*(properties, path) {
  if (properties.size !== undefined) {
    let stat = yield OS.File.stat(path);
    if (stat.size != properties.size) {
      throw new Error("Downloaded file was " + stat.size + " bytes but expected " + properties.size + " bytes.");
    }
  }

  if (properties.hashFunction !== undefined) {
    let expectedDigest = properties.hashValue.toLowerCase();
    let digest = yield computeHash(properties.hashFunction, path);
    if (digest != expectedDigest) {
      throw new Error("Hash was `" + digest + "` but expected `" + expectedDigest +  "`.");
    }
  }
});

const ProductAddonChecker = {
  /**
   * Downloads a list of add-ons from a URL optionally testing the SSL
   * certificate for certain attributes.
   *
   * @param  url
   *         The url to download from.
   * @param  allowNonBuiltIn
   *         Whether to trust SSL certificates without a built-in CA issuer.
   * @param  allowedCerts
   *         The list of certificate attributes to match the SSL certificate
   *         against or null to skip checks.
   * @return a promise that resolves to an object containing the list of add-ons
   *         and whether the local fallback was used, or rejects with a JS
   *         exception in case of error.
   */
  getProductAddonList(url, allowNonBuiltIn = false, allowedCerts = null) {
    return downloadXML(url, allowNonBuiltIn, allowedCerts)
      .then(parseXML)
      .catch(downloadLocalConfig);
  },

  /**
   * Downloads an add-on to a local file and checks that it matches the expected
   * file. The caller is responsible for deleting the temporary file returned.
   *
   * @param  addon
   *         The addon to download.
   * @return a promise that resolves to the temporary file downloaded or rejects
   *         with a JS exception in case of error.
   */
  downloadAddon: Task.async(function*(addon) {
    let path = yield downloadFile(addon.URL);
    try {
      yield verifyFile(addon, path);
      return path;
    }
    catch (e) {
      yield OS.File.remove(path);
      throw e;
    }
  })
}
