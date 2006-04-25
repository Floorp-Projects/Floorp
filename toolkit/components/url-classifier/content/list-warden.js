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
 *   Niels Provos <niels@google.com> (original author)d

 *   Fritz Schneider <fritz@google.com>
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

// A warden that knows how to register lists with a listmanager and keep them
// updated if necessary.  The ListWarden also provides a simple interface to
// check if a URL is evil or not.  Specialized wardens like the PhishingWarden
// inherit from it.
//
// Classes that inherit from ListWarden are responsible for calling
// enableTableUpdates or disableTableUpdates.  This usually entails
// registering prefObservers and calling enable or disable in the base
// class as appropriate.
//

/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @param ListManager ListManager that allows us to check against white 
 *                    and black lists.
 *
 * @constructor
 */
function PROT_ListWarden(listManager) {
  this.debugZone = "listwarden";
  this.listManager_ = listManager;

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // Once we register tables, their respective names will be listed here.
  this.blackTables_ = [];
  this.whiteTables_ = [];
  this.sandboxUpdates_ = [];
}

/**
 * Tell the ListManger to keep all of our tables updated
 */

PROT_ListWarden.prototype.enableTableUpdates = function() {
  this.listManager_.enableUpdateTables(this.blackTables_);
  this.listManager_.enableUpdateTables(this.whiteTables_);
}

/**
 * Tell the ListManager to stop updating our tables
 */

PROT_ListWarden.prototype.disableTableUpdates = function() {
  this.listManager_.disableUpdateTables(this.blackTables_);
  this.listManager_.disableUpdateTables(this.whiteTables_);
}

/**
 * Tell the ListManager to keep sandbox updated.  Don't tie this to the wl/bl
 * updates.
 */
PROT_ListWarden.prototype.enableSandboxUpdates = function() {
  this.listManager_.enableUpdateTables(this.sandboxUpdates_);
}

/**
 * Tell the ListManager not to keep sandbox updated.  Don't tie this to the
 * wl/bl updates.
 */
PROT_ListWarden.prototype.disableSandboxUpdates = function() {
  this.listManager_.disableUpdateTables(this.sandboxUpdates_);
}

/**
 * Register a new black list table with the list manager
 * @param tableName - name of the table to register
 * @returns true if the table could be registered, false otherwise
 */

PROT_ListWarden.prototype.registerBlackTable = function(tableName) {
  var result = this.listManager_.registerTable(tableName);
  if (result)
    this.blackTables_.push(tableName);
  return result;
}

/**
 * Register a new white list table with the list manager
 * @param tableName - name of the table to register
 * @returns true if the table could be registered, false otherwise
 */

PROT_ListWarden.prototype.registerWhiteTable = function(tableName) {
  var result = this.listManager_.registerTable(tableName);
  if (result)
    this.whiteTables_.push(tableName);
  return result;
}

/**
 * Internal method that looks up a url in a set of tables
 *
 * @param tables array of table names
 * @param url URL to look up
 * @returns Boolean indicating if the url is in one of the tables
 */
PROT_ListWarden.prototype.isURLInTables_ = function(tables, url) {
  for (var i = 0; i < tables.length; ++i) {
    if (this.listManager_.safeLookup(tables[i], url))
      return true;
  }
  return false;
}

/**
 * Internal method that looks up a url in the white list.
 *
 * @param url URL to look up
 * @returns Boolean indicating if the url is on our whitelist
 */
PROT_ListWarden.prototype.isWhiteURL_ = function(url) {
  if (!this.listManager_)
    return false;

  var whitelisted = this.isURLInTables_(this.whiteTables_, url);

  if (whitelisted)
    G_Debug(this, "Whitelist hit: " + url);

  return whitelisted;
}

/**
 * Internal method that looks up a url in the black lists.
 *
 * @param url URL to look up
 * @returns Boolean indicating if the url is on our blacklist(s)
 */
PROT_ListWarden.prototype.isBlackURL_ = function(url) {
  if (!this.listManager_)
    return false;

  var blacklisted = this.isURLInTables_(this.blackTables_, url);

  if (blacklisted)
    G_Debug(this, "Blacklist hit: " + url);

  return blacklisted;
}

/**
 * Internal method that looks up a url in both the white and black lists.
 *
 * If there is conflict, the white list has precedence over the black list.
 *
 * @param url URL to look up
 * @returns Boolean indicating if the url is phishy.
 */
PROT_ListWarden.prototype.isEvilURL_ = function(url) {
  return !this.isWhiteURL_(url) && this.isBlackURL_(url);
}

// Some unittests 
// TODO something more appropriate

function TEST_PROT_ListWarden() {
  if (G_GDEBUG) {
    var z = "listwarden UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    var threadQueue = new TH_ThreadQueue();
    var listManager = new PROT_ListManager(threadQueue, true /* testing */); 
    
    var warden = new PROT_ListWarden(listManager);

    // Just some really simple test
    G_Assert(z, warden.registerWhiteTable("test-white-domain"),
             "Failed to register test-white-domain table");
    G_Assert(z, warden.registerWhiteTable("test-black-url"),
             "Failed to register test-black-url table");
    listManager.safeInsert("test-white-domain", "http://foo.com/good", "1");
    listManager.safeInsert("test-black-url", "http://foo.com/good/1", "1");
    listManager.safeInsert("test-black-url", "http://foo.com/bad/1", "1");

    G_Assert(z, !warden.isEvilURL_("http://foo.com/good/1"),
             "White listing is not working.");
    G_Assert(z, warden.isEvilURL_("http://foo.com/bad/1"),
             "Black listing is not working.");

    G_Debug(z, "PASSED");
  }
}
