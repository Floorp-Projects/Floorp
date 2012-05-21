# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XXX: This should all be moved into the dbservice class so it happens
// in the background thread.

/**
 * Abstract base class for a lookup table.
 * @construction
 */
function UrlClassifierTable() {
  this.debugZone = "urlclassifier-table";
  this.name = '';
  this.needsUpdate = false;
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();
  this.wrappedJSObject = this;
}

UrlClassifierTable.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsISupports) ||
      iid.equals(Components.interfaces.nsIUrlClassifierTable))
    return this;

  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Subclasses need to implement this method.
 */
UrlClassifierTable.prototype.exists = function(url, callback) {
  throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////////////////////////////
// Url table implementation
function UrlClassifierTableUrl() {
  UrlClassifierTable.call(this);
}
UrlClassifierTableUrl.inherits(UrlClassifierTable);

/**
 * Look up a URL in a URL table
 */
UrlClassifierTableUrl.prototype.exists = function(url, callback) {
  // nsIUrlClassifierUtils.canonicalizeURL is the old way of canonicalizing a
  // URL.  Unfortunately, it doesn't normalize numeric domains so alternate IP
  // formats (hex, octal, etc) won't trigger a match.
  // this.enchashDecrypter_.getCanonicalUrl does the right thing and
  // normalizes a URL to 4 decimal numbers, but the update server may still be
  // giving us encoded IP addresses.  So to be safe, we check both cases.
  var urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                 .getService(Ci.nsIUrlClassifierUtils);
  var oldCanonicalized = urlUtils.canonicalizeURL(url);
  var canonicalized = this.enchashDecrypter_.getCanonicalUrl(url);
  G_Debug(this, "Looking up: " + url + " (" + oldCanonicalized + " and " +
                canonicalized + ")");
  (new ExistsMultiQuerier([oldCanonicalized, canonicalized],
                          this.name,
                          callback)).run();
}

/////////////////////////////////////////////////////////////////////
// Domain table implementation

function UrlClassifierTableDomain() {
  UrlClassifierTable.call(this);
  this.debugZone = "urlclassifier-table-domain";
  this.ioService_ = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
}
UrlClassifierTableDomain.inherits(UrlClassifierTable);

/**
 * Look up a URL in a domain table
 * We also try to lookup domain + first path component (e.g.,
 * www.mozilla.org/products).
 *
 * @returns Boolean true if the url domain is in the table
 */
UrlClassifierTableDomain.prototype.exists = function(url, callback) {
  var canonicalized = this.enchashDecrypter_.getCanonicalUrl(url);
  var urlObj = this.ioService_.newURI(canonicalized, null, null);
  var host = '';
  try {
    host = urlObj.host;
  } catch (e) { }
  var hostComponents = host.split(".");

  // Try to get the path of the URL.  Pseudo urls (like wyciwyg:) throw
  // errors when trying to convert to an nsIURL so we wrap in a try/catch
  // block.
  var path = ""
  try {
    urlObj.QueryInterface(Ci.nsIURL);
    path = urlObj.filePath;
  } catch (e) { }

  var pathComponents = path.split("/");

  // We don't have a good way map from hosts to domains, so we instead try
  // each possibility. Could probably optimize to start at the second dot?
  var possible = [];
  for (var i = 0; i < hostComponents.length - 1; i++) {
    host = hostComponents.slice(i).join(".");
    possible.push(host);

    // The path starts with a "/", so we are interested in the second path
    // component if it is available
    if (pathComponents.length >= 2 && pathComponents[1].length > 0) {
      host = host + "/" + pathComponents[1];
      possible.push(host);
    }
  }

  // Run the possible domains against the db.
  (new ExistsMultiQuerier(possible, this.name, callback)).run();
}

/////////////////////////////////////////////////////////////////////
// Enchash table implementation

function UrlClassifierTableEnchash() {
  UrlClassifierTable.call(this);
  this.debugZone = "urlclassifier-table-enchash";
}
UrlClassifierTableEnchash.inherits(UrlClassifierTable);

/**
 * Look up a URL in an enchashDB.  We try all sub domains (up to MAX_DOTS).
 */
UrlClassifierTableEnchash.prototype.exists = function(url, callback) {
  url = this.enchashDecrypter_.getCanonicalUrl(url);
  var host = this.enchashDecrypter_.getCanonicalHost(url,
                                               PROT_EnchashDecrypter.MAX_DOTS);

  var possible = [];
  for (var i = 0; i < PROT_EnchashDecrypter.MAX_DOTS + 1; i++) {
    possible.push(host);

    var index = host.indexOf(".");
    if (index == -1)
      break;
    host = host.substring(index + 1);
  }
  // Run the possible domains against the db.
  (new EnchashMultiQuerier(possible, this.name, callback, url)).run();
}
