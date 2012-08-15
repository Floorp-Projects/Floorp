# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


// A class that manages lists, namely white and black lists for
// phishing or malware protection. The ListManager knows how to fetch,
// update, and store lists.
//
// There is a single listmanager for the whole application.
//
// TODO more comprehensive update tests, for example add unittest check 
//      that the listmanagers tables are properly written on updates

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
  this.updateInterval = this.prefs_.getPref("urlclassifier.updateinterval", 30 * 60) * 1000;

  this.updateserverURL_ = null;
  this.gethashURL_ = null;

  this.isTesting_ = false;

  this.tablesData = {};

  this.observerServiceObserver_ = new G_ObserverServiceObserver(
                                          'quit-application',
                                          BindToObject(this.shutdown_, this),
                                          true /*only once*/);

  // Lazily create the key manager (to avoid fetching keys when they
  // aren't needed).
  this.keyManager_ = null;

  this.rekeyObserver_ = new G_ObserverServiceObserver(
                                          'url-classifier-rekey-requested',
                                          BindToObject(this.rekey_, this),
                                          false);
  this.updateWaitingForKey_ = false;

  this.cookieObserver_ = new G_ObserverServiceObserver(
                                          'cookie-changed',
                                          BindToObject(this.cookieChanged_, this),
                                          false);

  /* Backoff interval should be between 30 and 60 minutes. */
  var backoffInterval = 30 * 60 * 1000;
  backoffInterval += Math.floor(Math.random() * (30 * 60 * 1000));

  this.requestBackoff_ = new RequestBackoff(2 /* max errors */,
                                      60*1000 /* retry interval, 1 min */,
                                            4 /* num requests */,
                                   60*60*1000 /* request time, 60 min */,
                              backoffInterval /* backoff interval, 60 min */,
                                 8*60*60*1000 /* max backoff, 8hr */);

  this.dbService_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                   .getService(Ci.nsIUrlClassifierDBService);

  this.hashCompleter_ = Cc["@mozilla.org/url-classifier/hashcompleter;1"]
                        .getService(Ci.nsIUrlClassifierHashCompleter);
}

/**
 * xpcom-shutdown callback
 * Delete all of our data tables which seem to leak otherwise.
 */
PROT_ListManager.prototype.shutdown_ = function() {
  if (this.keyManager_) {
    this.keyManager_.shutdown();
  }

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
 * Set the gethash url.
 */
PROT_ListManager.prototype.setGethashUrl = function(url) {
  G_Debug(this, "Set gethash url: " + url);
  if (url != this.gethashURL_) {
    this.gethashURL_ = url;
    this.hashCompleter_.gethashUrl = url;
  }
}

/**
 * Set the crypto key url.
 * @param url String
 */
PROT_ListManager.prototype.setKeyUrl = function(url) {
  G_Debug(this, "Set key url: " + url);
  if (!this.keyManager_) {
    this.keyManager_ = new PROT_UrlCryptoKeyManager();
    this.keyManager_.onNewKey(BindToObject(this.newKey_, this));

    this.hashCompleter_.setKeys(this.keyManager_.getClientKey(),
                                this.keyManager_.getWrappedKey());
  }

  this.keyManager_.setKeyUrl(url);
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
 * hour).  The first update is scheduled for a random time between 0.5 and 1.5
 * times the update interval.
 */
PROT_ListManager.prototype.startUpdateChecker = function() {
  this.stopUpdateChecker();

  // Schedule the first check for between 15 and 45 minutes.
  var repeatingUpdateDelay = this.updateInterval / 2;
  repeatingUpdateDelay += Math.floor(Math.random() * this.updateInterval);
  this.updateChecker_ = new G_Alarm(BindToObject(this.initialUpdateCheck_,
                                                 this),
                                    repeatingUpdateDelay);
}

/**
 * Callback for the first update check.
 * We go ahead and check for table updates, then start a regular timer (once
 * every update interval).
 */
PROT_ListManager.prototype.initialUpdateCheck_ = function() {
  this.checkForUpdates();
  this.updateChecker_ = new G_Alarm(BindToObject(this.checkForUpdates, this), 
                                    this.updateInterval, true /* repeat */);
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
 * @param key Principal being used to lookup the database
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
  if (!this.keyManager_)
    return;

  if (!this.keyManager_.hasKey()) {
    // We don't have a client key yet.  Schedule a rekey, and rerequest
    // when we have one.

    // If there's already an update waiting for a new key, don't bother.
    if (this.updateWaitingForKey_)
      return;

    // If maybeReKey() returns false we have asked for too many keys,
    // and won't be getting a new one.  Since we don't want to do
    // updates without a client key, we'll skip this update if maybeReKey()
    // fails.
    if (this.keyManager_.maybeReKey())
      this.updateWaitingForKey_ = true;

    return;
  }

  var tableList;
  var tableNames = {};
  for (var tableName in this.tablesData) {
    if (this.tablesData[tableName].needsUpdate)
      tableNames[tableName] = true;
    if (!tableList) {
      tableList = tableName;
    } else {
      tableList += "," + tableName;
    }
  }

  var request = "";

  // For each table already in the database, include the chunk data from
  // the database
  var lines = tableData.split("\n");
  for (var i = 0; i < lines.length; i++) {
    var fields = lines[i].split(";");
    if (tableNames[fields[0]]) {
      request += lines[i] + ":mac\n";
      delete tableNames[fields[0]];
    }
  }

  // For each requested table that didn't have chunk data in the database,
  // request it fresh
  for (var tableName in tableNames) {
    request += tableName + ";mac\n";
  }

  G_Debug(this, 'checkForUpdates: scheduling request..');
  var streamer = Cc["@mozilla.org/url-classifier/streamupdater;1"]
                 .getService(Ci.nsIUrlClassifierStreamUpdater);
  try {
    streamer.updateUrl = this.updateserverURL_ +
                         "&wrkey=" + this.keyManager_.getWrappedKey();
  } catch (e) {
    G_Debug(this, 'invalid url');
    return;
  }

  this.requestBackoff_.noteRequest();

  if (!streamer.downloadUpdates(tableList,
                                request,
                                this.keyManager_.getClientKey(),
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

  // Let the backoff object know that we completed successfully.
  this.requestBackoff_.noteServerResponse(200);
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

  if (this.requestBackoff_.isErrorStatus(status)) {
    // Schedule an update for when our backoff is complete
    this.currentUpdateChecker_ =
      new G_Alarm(BindToObject(this.checkForUpdates, this),
                  this.requestBackoff_.nextRequestDelay());
  }
}

/**
 * Called when either the update process or a gethash request signals
 * that the server requested a rekey.
 */
PROT_ListManager.prototype.rekey_ = function() {
  G_Debug(this, "rekey requested");

  // The current key is no good anymore.
  this.keyManager_.dropKey();
  this.keyManager_.maybeReKey();
}

/**
 * Called when cookies are cleared - clears the current MAC keys.
 */
PROT_ListManager.prototype.cookieChanged_ = function(subject, topic, data) {
  if (data != "cleared")
    return;

  G_Debug(this, "cookies cleared");
  this.keyManager_.dropKey();
}

/**
 * Called when we've received a new key from the server.
 */
PROT_ListManager.prototype.newKey_ = function() {
  G_Debug(this, "got a new MAC key");

  this.hashCompleter_.setKeys(this.keyManager_.getClientKey(),
                              this.keyManager_.getWrappedKey());

  if (this.keyManager_.hasKey()) {
    if (this.updateWaitingForKey_) {
      this.updateWaitingForKey_ = false;
      this.checkForUpdates();
    }
  }
}

PROT_ListManager.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsIUrlListManager) ||
      iid.equals(Ci.nsITimerCallback))
    return this;

  throw Components.results.NS_ERROR_NO_INTERFACE;
}
