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
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Niels Provos <niels@google.com> (original author)
#   Fritz Schneider <fritz@google.com>
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


// A class that manages lists, namely white and black lists for
// phishing or malware protection. The ListManager knows how to fetch,
// update, and store lists.
//
// There is a single listmanager for the whole application.
//
// TODO more comprehensive update tests, for example add unittest check 
//      that the listmanagers tables are properly written on updates

// How frequently we check for updates (30 minutes)
const kUpdateInterval = 30 * 60 * 1000;

function QueryAdapter(callback) {
  this.callback_ = callback;
};

QueryAdapter.prototype.handleResponse = function(value) {
  this.callback_.handleEvent(value);
}

/**
 * A ListManager keeps track of black and white lists and knows
 * how to update them.
 *
 * @constructor
 */
function PROT_ListManager() {
  this.debugZone = "listmanager";
  G_debugService.enableZone(this.debugZone);

  this.currentUpdateChecker_ = null;   // set when we toggle updates
  this.prefs_ = new G_Preferences();

  this.updateserverURL_ = null;

  this.isTesting_ = false;

  this.tablesData = {};

  this.observerServiceObserver_ = new G_ObserverServiceObserver(
                                          'xpcom-shutdown',
                                          BindToObject(this.shutdown_, this),
                                          true /*only once*/);

  // Lazily create urlCrypto (see tr-fetcher.js)
  this.urlCrypto_ = null;
  
  this.requestBackoff_ = new RequestBackoff(3 /* num errors */,
                                   10*60*1000 /* error time, 10min */,
                                   60*60*1000 /* backoff interval, 60min */,
                                   6*60*60*1000 /* max backoff, 6hr */);

  this.dbService_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                   .getService(Ci.nsIUrlClassifierDBService);
}

/**
 * xpcom-shutdown callback
 * Delete all of our data tables which seem to leak otherwise.
 */
PROT_ListManager.prototype.shutdown_ = function() {
  for (var name in this.tablesData) {
    delete this.tablesData[name];
  }
}

/**
 * Set the url we check for updates.  If the new url is valid and different,
 * update our table list.
 * 
 * After setting the update url, the caller is responsible for registering
 * tables and then toggling update checking.  All the code for this logic is
 * currently in browser/components/safebrowsing.  Maybe it should be part of
 * the listmanger?
 */
PROT_ListManager.prototype.setUpdateUrl = function(url) {
  G_Debug(this, "Set update url: " + url);
  if (url != this.updateserverURL_) {
    this.updateserverURL_ = url;
    this.requestBackoff_.reset();
    
    // Remove old tables which probably aren't valid for the new provider.
    for (var name in this.tablesData) {
      delete this.tablesData[name];
    }
  }
}

/**
 * Set the crypto key url.
 * @param url String
 */
PROT_ListManager.prototype.setKeyUrl = function(url) {
  G_Debug(this, "Set key url: " + url);
  if (!this.urlCrypto_)
    this.urlCrypto_ = new PROT_UrlCrypto();
  
  this.urlCrypto_.manager_.setKeyUrl(url);
}

/**
 * Register a new table table
 * @param tableName - the name of the table
 * @param opt_requireMac true if a mac is required on update, false otherwise
 * @returns true if the table could be created; false otherwise
 */
PROT_ListManager.prototype.registerTable = function(tableName, 
                                                    opt_requireMac) {
  this.tablesData[tableName] = {};
  this.tablesData[tableName].needsUpdate = false;

  return true;
}

/**
 * Enable updates for some tables
 * @param tables - an array of table names that need updating
 */
PROT_ListManager.prototype.enableUpdate = function(tableName) {
  var changed = false;
  var table = this.tablesData[tableName];
  if (table) {
    G_Debug(this, "Enabling table updates for " + tableName);
    table.needsUpdate = true;
    changed = true;
  }

  if (changed === true)
    this.maybeToggleUpdateChecking();
}

/**
 * Disables updates for some tables
 * @param tables - an array of table names that no longer need updating
 */
PROT_ListManager.prototype.disableUpdate = function(tableName) {
  var changed = false;
  var table = this.tablesData[tableName];
  if (table) {
    G_Debug(this, "Disabling table updates for " + tableName);
    table.needsUpdate = false;
    changed = true;
  }

  if (changed === true)
    this.maybeToggleUpdateChecking();
}

/**
 * Determine if we have some tables that need updating.
 */
PROT_ListManager.prototype.requireTableUpdates = function() {
  for (var type in this.tablesData) {
    // Tables that need updating even if other tables dont require it
    if (this.tablesData[type].needsUpdate)
      return true;
  }

  return false;
}

/**
 * Start managing the lists we know about. We don't do this automatically
 * when the listmanager is instantiated because their profile directory
 * (where we store the lists) might not be available.
 */
PROT_ListManager.prototype.maybeStartManagingUpdates = function() {
  if (this.isTesting_)
    return;

  // We might have been told about tables already, so see if we should be
  // actually updating.
  this.maybeToggleUpdateChecking();
}

PROT_ListManager.prototype.kickoffUpdate_ = function (tableData)
{
  this.startingUpdate_ = false;
  // If the user has never downloaded tables, do the check now.
  // If the user has tables, add a fuzz of a few minutes.
  var initialUpdateDelay = 3000;
  if (tableData != "") {
    // Add a fuzz of 0-5 minutes.
    initialUpdateDelay += Math.floor(Math.random() * (5 * 60 * 1000));
  }

  this.currentUpdateChecker_ =
    new G_Alarm(BindToObject(this.checkForUpdates, this),
                initialUpdateDelay);
}

/**
 * Determine if we have any tables that require updating.  Different
 * Wardens may call us with new tables that need to be updated.
 */ 
PROT_ListManager.prototype.maybeToggleUpdateChecking = function() {
  // If we are testing or dont have an application directory yet, we should
  // not start reading tables from disk or schedule remote updates
  if (this.isTesting_)
    return;

  // We update tables if we have some tables that want updates.  If there
  // are no tables that want to be updated - we dont need to check anything.
  if (this.requireTableUpdates() === true) {
    G_Debug(this, "Starting managing lists");
    this.startUpdateChecker();

    // Multiple warden can ask us to reenable updates at the same time, but we
    // really just need to schedule a single update.
    if (!this.currentUpdateChecker && !this.startingUpdate_) {
      this.startingUpdate_ = true;
      // check the current state of tables in the database
      this.dbService_.getTables(BindToObject(this.kickoffUpdate_, this));
    }
  } else {
    G_Debug(this, "Stopping managing lists (if currently active)");
    this.stopUpdateChecker();                    // Cancel pending updates
  }
}

/**
 * Start periodic checks for updates. Idempotent.
 * We want to distribute update checks evenly across the update period (an
 * hour).  To do this, we pick a random number of time between 0 and 30
 * minutes.  The client first checks at 15 + rand, then every 30 minutes after
 * that.
 */
PROT_ListManager.prototype.startUpdateChecker = function() {
  this.stopUpdateChecker();
  
  // Schedule the first check for between 15 and 45 minutes.
  var repeatingUpdateDelay = kUpdateInterval / 2;
  repeatingUpdateDelay += Math.floor(Math.random() * kUpdateInterval);
  this.updateChecker_ = new G_Alarm(BindToObject(this.initialUpdateCheck_,
                                                 this),
                                    repeatingUpdateDelay);
}

/**
 * Callback for the first update check.
 * We go ahead and check for table updates, then start a regular timer (once
 * every 30 minutes).
 */
PROT_ListManager.prototype.initialUpdateCheck_ = function() {
  this.checkForUpdates();
  this.updateChecker_ = new G_Alarm(BindToObject(this.checkForUpdates, this), 
                                    kUpdateInterval, true /* repeat */);
}

/**
 * Stop checking for updates. Idempotent.
 */
PROT_ListManager.prototype.stopUpdateChecker = function() {
  if (this.updateChecker_) {
    this.updateChecker_.cancel();
    this.updateChecker_ = null;
  }
  // Cancel the oneoff check from maybeToggleUpdateChecking.
  if (this.currentUpdateChecker_) {
    this.currentUpdateChecker_.cancel();
    this.currentUpdateChecker_ = null;
  }
}

/**
 * Provides an exception free way to look up the data in a table. We
 * use this because at certain points our tables might not be loaded,
 * and querying them could throw.
 *
 * @param table String Name of the table that we want to consult
 * @param key String Key for table lookup
 * @param callback nsIUrlListManagerCallback (ie., Function) given false or the
 *        value in the table corresponding to key.  If the table name does not
 *        exist, we return false, too.
 */
PROT_ListManager.prototype.safeLookup = function(key, callback) {
  try {
    G_Debug(this, "safeLookup: " + key);
    var cb = new QueryAdapter(callback);
    this.dbService_.lookup(key,
                           BindToObject(cb.handleResponse, cb),
                           true);
  } catch(e) {
    G_Debug(this, "safeLookup masked failure for key " + key + ": " + e);
    callback.handleEvent("");
  }
}

/**
 * Updates our internal tables from the update server
 *
 * @returns true when a new request was scheduled, false if an old request
 *          was still pending.
 */
PROT_ListManager.prototype.checkForUpdates = function() {
  // Allow new updates to be scheduled from maybeToggleUpdateChecking()
  this.currentUpdateChecker_ = null;

  if (!this.updateserverURL_) {
    G_Debug(this, 'checkForUpdates: no update server url');
    return false;
  }

  // See if we've triggered the request backoff logic.
  if (!this.requestBackoff_.canMakeRequest())
    return false;

  // Grab the current state of the tables from the database
  this.dbService_.getTables(BindToObject(this.makeUpdateRequest_, this));
  return true;
}

/**
 * Method that fires the actual HTTP update request.
 * First we reset any tables that have disappeared.
 * @param tableData List of table data already in the database, in the form
 *        tablename;<chunk ranges>\n
 */
PROT_ListManager.prototype.makeUpdateRequest_ = function(tableData) {
  var tableNames = {};
  for (var tableName in this.tablesData) {
    if (this.tablesData[tableName].needsUpdate)
      tableNames[tableName] = true;
  }

  var request = "";

  // For each table already in the database, include the chunk data from
  // the database
  var lines = tableData.split("\n");
  for (var i = 0; i < lines.length; i++) {
    var fields = lines[i].split(";");
    if (tableNames[fields[0]]) {
      request += lines[i] + "\n";
      delete tableNames[fields[0]];
    }
  }

  // For each requested table that didn't have chunk data in the database,
  // request it fresh
  for (var tableName in tableNames) {
    request += tableName + ";\n";
  }

  G_Debug(this, 'checkForUpdates: scheduling request..');
  var streamer = Cc["@mozilla.org/url-classifier/streamupdater;1"]
                 .getService(Ci.nsIUrlClassifierStreamUpdater);
  try {
    streamer.updateUrl = this.updateserverURL_;
  } catch (e) {
    G_Debug(this, 'invalid url');
    return;
  }

  if (!streamer.downloadUpdates(request,
                                BindToObject(this.updateSuccess_, this),
                                BindToObject(this.updateError_, this),
                                BindToObject(this.downloadError_, this))) {
    G_Debug(this, "pending update, wait until later");
  }
}

/**
 * Callback function if the update request succeeded.
 * @param waitForUpdate String The number of seconds that the client should
 *        wait before requesting again.
 */
PROT_ListManager.prototype.updateSuccess_ = function(waitForUpdate) {
  G_Debug(this, "update success: " + waitForUpdate);
  if (waitForUpdate) {
    var delay = parseInt(waitForUpdate, 10);
    // As long as the delay is something sane (5 minutes or more), update
    // our delay time for requesting updates
    if (delay >= (5 * 60) && this.updateChecker_)
      this.updateChecker_.setDelay(delay * 1000);
  }
}

/**
 * Callback function if the update request succeeded.
 * @param result String The error code of the failure
 */
PROT_ListManager.prototype.updateError_ = function(result) {
  G_Debug(this, "update error: " + result);
  // XXX: there was some trouble applying the updates.
}

/**
 * Callback function when the download failed
 * @param status String http status or an empty string if connection refused.
 */
PROT_ListManager.prototype.downloadError_ = function(status) {
  G_Debug(this, "download error: " + status);
  // If status is empty, then we assume that we got an NS_CONNECTION_REFUSED
  // error.  In this case, we treat this is a http 500 error.
  if (!status) {
    status = 500;
  }
  status = parseInt(status, 10);
  this.requestBackoff_.noteServerResponse(status);

  // Try again in a minute
  this.currentUpdateChecker_ =
    new G_Alarm(BindToObject(this.checkForUpdates, this), 60000);
}

PROT_ListManager.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsIUrlListManager) ||
      iid.equals(Ci.nsITimerCallback))
    return this;

  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;
}
