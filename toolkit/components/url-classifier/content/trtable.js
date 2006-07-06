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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@ponderer.org>
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
}

UrlClassifierTable.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsISupports) ||
      iid.equals(Components.interfaces.nsIUrlClassifierTable))
    return this;                                              
  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;
}

/**
 * Subclasses need to implment this method.
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
  var canonicalized = PROT_URLCanonicalizer.canonicalizeURL_(url);
  G_Debug(this, "Looking up: " + url + " (" + canonicalized + ")");

  var dbservice_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                   .getService(Ci.nsIUrlClassifierDBService);
  var callbackHelper = new UrlLookupCallback(callback);
  dbservice_.exists(this.name,
                    canonicalized,
                    BindToObject(callbackHelper.dbCallback,
                                 callbackHelper));
}

/**
 * A helper class for handling url lookups in the database.  This allows us to
 * break our reference to callback to avoid memory leaks.
 * @param callback nsIUrlListManagerCallback
 */
function UrlLookupCallback(callback) {
  this.callback_ = callback;
}

/**
 * Callback function from nsIUrlClassifierDBService.exists.  For url lookup,
 * any non-empty string value is a database hit.
 * @param value String
 */
UrlLookupCallback.prototype.dbCallback = function(value) {
  this.callback_.handleEvent(value.length > 0);
  this.callback_ = null;
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
 *
 * @returns Boolean true if the url domain is in the table
 */
UrlClassifierTableDomain.prototype.exists = function(url, callback) {
  var urlObj = this.ioService_.newURI(url, null, null);
  var host = '';
  try {
    host = urlObj.host;
  } catch (e) { }
  var components = host.split(".");

  // We don't have a good way map from hosts to domains, so we instead try
  // each possibility. Could probably optimize to start at the second dot?
  var possible = [];
  for (var i = 0; i < components.length - 1; i++) {
    host = components.slice(i).join(".");
    possible.push(host);
  }

  // Run the possible domains against the db.
  (new ExistsMultiQuerier(possible, this.name, callback)).run();
}

/////////////////////////////////////////////////////////////////////
// Enchash table implementation

function UrlClassifierTableEnchash() {
  UrlClassifierTable.call(this);
  this.debugZone = "urlclassifier-table-enchash";
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();
}
UrlClassifierTableEnchash.inherits(UrlClassifierTable);

/**
 * Look up a URL in an enchashDB.  We try all sub domains (up to MAX_DOTS).
 */
UrlClassifierTableEnchash.prototype.exists = function(url, callback) {
  var host = this.enchashDecrypter_.getCanonicalHost(url);

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
