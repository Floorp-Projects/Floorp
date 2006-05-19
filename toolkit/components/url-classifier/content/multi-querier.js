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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@google.com> (original author)
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

/**
 * This class helps us batch a series of async calls to the db.
 * If any of the tokens is in the database, we fire callback with
 * true as a param.  If all the tokens are not in the  database,
 * we fire callback with false as a param.
 * This is an "Abstract" base class.  Subclasses need to supply
 * the condition_ method.
 *
 * @param tokens Array of strings to lookup in the db
 * @param tableName String name of the table
 * @param callback Function callback function that takes true if the condition
 *        passes.
 */
function MultiQuerier(tokens, tableName, callback) {
  this.tokens_ = tokens;
  this.tableName_ = tableName;
  this.callback_ = callback;
  this.dbservice_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                    .getService(Ci.nsIUrlClassifierDBService);
  // We put the current token in this variable.
  this.key_ = null;
}

/**
 * Run the remaining tokens against the db.
 */
MultiQuerier.prototype.run = function() {
  if (this.tokens_.length == 0) {
    this.callback_.handleEvent(false);
    this.dbservice_ = null;
    this.callback_ = null;
    return;
  }
  
  this.key_ = this.tokens_.pop();
  G_Debug(this, "Looking up " + this.key_ + " in " + this.tableName_);
  this.dbservice_.exists(this.tableName_, this.key_,
                         BindToObject(this.result_, this));
}

/**
 * Callback from the db.  If the returned value passes the this.condition_
 * test, go ahead and call the main callback.
 */
MultiQuerier.prototype.result_ = function(value) {
  if (this.condition_(value)) {
    this.callback_.handleEvent(true)
    this.dbservice_ = null;
    this.callback_ = null;
  } else {
    this.run();
  }
}

// Subclasses must override this.
MultiQuerier.prototype.condition_ = function(value) {
  throw "MultiQuerier is an abstract base class";
}


/**
 * Concrete MultiQuerier that stops if the key exists in the db.
 */
function ExistsMultiQuerier(tokens, tableName, callback) {
  MultiQuerier.call(this, tokens, tableName, callback);
  this.debugZone = "existsMultiQuerier";
}
ExistsMultiQuerier.inherits(MultiQuerier);

ExistsMultiQuerier.prototype.condition_ = function(value) {
  return value.length > 0;
}


/**
 * Concrete MultiQuerier that looks up a key, decrypts it, then
 * checks the the resulting regular expressions for a match.
 * @param tokens Array of hosts
 */
function EnchashMultiQuerier(tokens, tableName, callback, url) {
  MultiQuerier.call(this, tokens, tableName, callback);
  this.url_ = url;
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();
  this.debugZone = "enchashMultiQuerier";
}
EnchashMultiQuerier.inherits(MultiQuerier);

EnchashMultiQuerier.prototype.run = function() {
  if (this.tokens_.length == 0) {
    this.callback_.handleEvent(false);
    this.dbservice_ = null;
    this.callback_ = null;
    return;
  }
  var host = this.tokens_.pop();
  this.key_ = host;
  var lookupKey = this.enchashDecrypter_.getLookupKey(host);
  this.dbservice_.exists(this.tableName_, lookupKey,
                         BindToObject(this.result_, this));
}

EnchashMultiQuerier.prototype.condition_ = function(encryptedValue) {
  if (encryptedValue.length > 0) {
    // We have encrypted regular expressions for this host. Let's 
    // decrypt them and see if we have a match.
    var decrypted = this.enchashDecrypter_.decryptData(encryptedValue,
                                                       this.key_);
    var res = this.enchashDecrypter_.parseRegExps(decrypted);
    for (var j = 0; j < res.length; j++) {
      if (res[j].test(this.url_)) {
        return true;
      }
    }
  }
  return false;
}
