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
 *   Niels Provos <niels@google.com> (original author)
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


// A class that manages lists, namely white and black lists for
// phishing or malware protection. The ListManager knows how to fetch,
// update, and store lists, and knows the "kind" of list each is (is
// it a whitelist? a blacklist? etc). However it doesn't know how the
// lists are serialized or deserialized (the wireformat classes know
// this) nor the specific format of each list. For example, the list
// could be a map of domains to "1" if the domain is phishy. Or it
// could be a map of hosts to regular expressions to match, who knows?
// Answer: the trtable knows. List are serialized/deserialized by the
// wireformat reader from/to trtables, and queried by the listmanager.
//
// There is a single listmanager for the whole application.
//
// The listmanager is used only in privacy mode; in advanced protection
// mode a remote server is queried.
//
// How to add a new table:
// 1) get it up on the server
// 2) add it to tablesKnown
// 3) if it is not a known table type (trtable.js), add an implementation
//    for it in trtable.js
// 4) add a check for it in the phishwarden's isXY() method, for example
//    isBlackURL()
//
// TODO: obviously the way this works could use a lot of improvement. In
//       particular adding a list should just be a matter of adding
//       its name to the listmanager and an implementation to trtable
//       (or not if a talbe of that type exists). The format and semantics
//       of the list comprise its name, so the listmanager should easily
//       be able to figure out what to do with what list (i.e., no
//       need for step 4).
// TODO reading, writing, and whatnot code should be cleaned up
// TODO read/write asynchronously
// TODO more comprehensive update tests, for example add unittest check 
//      that the listmanagers tables are properly written on updates


/**
 * A ListManager keeps track of black and white lists and knows
 * how to update them.
 *
 * @constructor
 */
function PROT_ListManager() {
  this.debugZone = "listmanager";
  G_debugService.enableZone(this.debugZone);

  // We run long-lived operations on "threads" (user-level
  // time-slices) in order to prevent locking up the UI
  var threadConfig = {
    "interleave": true,
    "runtime": 80,
    "delay": 400,
  };
  this.threadQueue_ = new TH_ThreadQueue(threadConfig);
  this.appDir_ = null;
  this.initAppDir_();   // appDir can be changed with setAppDir
  this.currentUpdateChecker_ = null;   // set when we toggle updates
  this.rpcPending_ = false;
  this.readingData_ = false;

  // We don't want to start checking against our lists until we've
  // read them. But then again, we don't want to queue URLs to check
  // forever.  So if we haven't successfully read our lists after a
  // certain amount of time, just pretend.
  this.dataImportedAtLeastOnce_ = false;
  var self = this;
  new G_Alarm(function() { self.dataImportedAtLeastOnce_ = true; }, 60 * 1000);

  // TOOD(tc): PREF
  this.updateserverURL_ = PROT_GlobalStore.getUpdateserverURL();

  // The lists we know about and the parses we can use to read
  // them. Default all to the earlies possible version (1.-1); this
  // version will get updated when successfully read from disk or
  // fetch updates.
  this.tablesKnown_ = {};
  
  if (PROT_GlobalStore.isTesting()) {
    // populate with some tables for unittesting
    this.tablesKnown_ = {
    //  "goog-white-domain": new PROT_VersionParser("goog-white-domain", 1, -1),
    //  "goog-black-url" : new PROT_VersionParser("goog-black-url", 1, -1),
    //  "goog-black-enchash": new PROT_VersionParser("goog-black-enchash", 1, -1),
    //  "goog-white-url": new PROT_VersionParser("goog-white-url", 1, -1),
    //
      // A major version of zero means local, so don't ask for updates
      "test1-foo-domain" : new PROT_VersionParser("test1-foo-domain", 0, -1),
      "test2-foo-domain" : new PROT_VersionParser("test2-foo-domain", 0, -1),
      "test-white-domain" : 
        new PROT_VersionParser("test-white-domain", 0, -1, true /* require mac*/),
      "test-mac-domain" :
        new PROT_VersionParser("test-mac-domain", 0, -1, true /* require mac */)
    };
    
    // expose the object for unittesting
    this.wrappedJSObject = this;
  }

  this.tablesData = {};

  this.urlCrypto_ = null;
}

/**
 * Register a new table table
 * @param tableName - the name of the table
 * @param opt_requireMac true if a mac is required on update, false otherwise
 * @returns true if the table could be created; false otherwise
 */
PROT_ListManager.prototype.registerTable = function(tableName, 
                                                    opt_requireMac) {
  var table = new PROT_VersionParser(tableName, 1, -1, opt_requireMac);
  if (!table)
    return false;
  this.tablesKnown_[tableName] = table;

  return true;
}

/**
 * Enable updates for some tables
 * @param tables - an array of table names that need updating
 */
PROT_ListManager.prototype.enableUpdate = function(tableName) {
  var changed = false;
  var table = this.tablesKnown_[tableName];
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
  var table = this.tablesKnown_[tableName];
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
  for (var type in this.tablesKnown_) {
    // All tables with a major of 0 are internal tables that we never
    // update remotely.
    if (this.tablesKnown_[type].major == 0)
      continue;
     
    // Tables that need updating even if other tables dont require it
    if (this.tablesKnown_[type].needsUpdate)
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
  if (PROT_GlobalStore.isTesting())
    return;

  // We might have been told about tables already, so see if we should be
  // actually updating.
  this.maybeToggleUpdateChecking();
}

/**
 * Determine if we have any tables that require updating.  Different
 * Wardens may call us with new tables that need to be updated.
 */ 
PROT_ListManager.prototype.maybeToggleUpdateChecking = function() {
  // If we are testing or dont have an application directory yet, we should
  // not start reading tables from disk or schedule remote updates
  if (PROT_GlobalStore.isTesting() || !this.appDir_)
    return;

  // We update tables if we have some tables that want updates.  If there
  // are no tables that want to be updated - we dont need to check anything.
  if (this.requireTableUpdates() === true) {
    G_Debug(this, "Starting managing lists");
    // Read new table data if necessary - if we already have data we just
    // skip reading from disk
    new G_Alarm(BindToObject(this.readDataFiles, this), 0);
    // Multiple warden can ask us to reenable updates at the same time, but we
    // really just need to schedule a single update.
    if (!this.currentUpdateChecker_)
      this.currentUpdateChecker_ =
        new G_Alarm(BindToObject(this.checkForUpdates, this), 3000);
    this.startUpdateChecker();
  } else {
    G_Debug(this, "Stopping managing lists (if currently active)");
    this.stopUpdateChecker();                    // Cancel pending updates
  }
}

/**
 * Start periodic checks for updates. Idempotent.
 */
PROT_ListManager.prototype.startUpdateChecker = function() {
  this.stopUpdateChecker();
  
  // Schedule a check for updates every so often
  // TODO(tc): PREF NEW
  var sixtyMinutes = 60 * 60 * 1000;
  this.updateChecker_ = new G_Alarm(BindToObject(this.checkForUpdates, this), 
                                    sixtyMinutes, true /* repeat */);
}

/**
 * Stop checking for updates. Idempotent.
 */
PROT_ListManager.prototype.stopUpdateChecker = function() {
  if (this.updateChecker_) {
    this.updateChecker_.cancel();
    this.updateChecker_ = null;
  }
}

/**
 * 
 */
PROT_ListManager.prototype.initAppDir_ = function() {
  var dir = G_File.getProfileFile();
  dir.append(PROT_GlobalStore.getAppDirectoryName());
  if (!dir.exists()) {
    try {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    } catch (e) {
      G_Error(this, dir.path + " couldn't be created.");
    }
  }
  this.setAppDir(dir);
}

/**
 * Set the directory in which we should serialize data
 *
 * @param appDir An nsIFile pointing to our directory (should exist)
 */
PROT_ListManager.prototype.setAppDir = function(appDir) {
  G_Debug(this, "setAppDir: " + appDir.path);
  this.appDir_ = appDir;
}

/**
 * Clears the specified table
 * @param table Name of the table that we want to consult
 * @returns true if the table exists, false otherwise
 */
PROT_ListManager.prototype.clearList = function(table) {
  if (!this.tablesKnown_[table])
    return false;

  this.tablesData[table] = newUrlClassifierTable(this.tablesKnown_[table].type);
  return true;
}

/**
 * Provides an exception free way to look up the data in a table. We
 * use this because at certain points our tables might not be loaded,
 * and querying them could throw.
 *
 * @param table Name of the table that we want to consult
 * @param key Key for table lookup
 * @returns false or the value in the table corresponding to key.
 *          If the table name does not exist, we return false, too.
 */
PROT_ListManager.prototype.safeExists = function(table, key) {
  var result = false;
  try {
    var map = this.tablesData[table];
    result = map.exists(key);
  } catch(e) {
    result = false;
    G_Debug(this, "safeExists masked failure for " + table + ", key " + key + ": " + e);
  }

  return result;
}

/**
 * Provides an exception free way to insert data into a table.
 * @param table Name of the table that we want to consult
 * @param key Key for table insert
 * @param value Value for table insert
 * @returns true if the value could be inserted, false otherwise
 */
PROT_ListManager.prototype.safeInsert = function(table, key, value) {
  if (!this.tablesKnown_[table]) {
    G_Debug(this, "Unknown table: " + table);
    return false;
  }
  if (!this.tablesData[table])
    this.tablesData[table] = newUrlClassifierTable(table);
  try {
    this.tablesData[table].insert(key, value);
  } catch (e) {
    G_Debug(this, "Cannot insert key " + key + " value " + value);
    G_Debug(this, e);
    return false;
  }

  return true;
}

/**
 * Provides an exception free way to remove data from a table.
 * @param table Name of the table that we want to consult
 * @param key Key for table erase
 * @returns true if the value could be removed, false otherwise
 */
PROT_ListManager.prototype.safeRemove = function(table, key) {
  if (!this.tablesKnown_[table]) {
    G_Debug(this, "Unknown table: " + table);
    return false;
  }

  if (!this.tablesData[table])
    return false;

  return this.tablesData[table].remove(key);
}

PROT_ListManager.prototype.getTable = function(tableName) {
  return this.tablesData[tableName];
}

/**
 * Returns a list of files that we should try to read.  We do not return
 * tables that already have data or that we tried to read in the past.
 * @returns array of file names
 */

PROT_ListManager.prototype.listUnreadDataFiles = function() {
  // Now we need to read all of our nice data files.
  var files = [];
  for (var type in this.tablesKnown_) {
    var table = this.tablesKnown_[type];
    // Do not read data for tables that we already know about
    if (this.tablesData[type] || table.didRead === true)
      continue;

    // Tables that dont require updates are not read from disk either
    if (table.needsUpdate === false)
      continue;

    var filename = type + ".sst";
    var file = this.appDir_.clone();
    file.append(filename);
    if (file.exists() && file.isFile() && file.isReadable()) {
      G_Debug(this, "Found saved data for: " + type);
      files.push(file);
    } else {
      G_Debug(this, "Failed to find saved data for: " + type);
    }
  }

  return files;
}

/**
 * Reads all data files from storage
 * @returns true if we started reading data from disk, false otherwise.
 */
PROT_ListManager.prototype.readDataFiles = function() {
  if (this.readingData_ === true) {
    G_Debug(this, "Already reading data from disk");
    return true;
  }

  if (this.rpcPending_ === true) {
    G_Debug(this, "Cannot read data files while an update RPC is pending");
    new G_Alarm(BindToObject(this.readDataFiles, this), 10 * 1000);
    return false;
  }

  // If we have no files there is nothing more for us todo
  var files = this.listUnreadDataFiles();
  if (!files.length)
    return true;

  this.readingData_ = true;

  // Remember that we attempted to read from all tables
  for (var type in this.tablesKnown_)
    this.tablesKnown_[type].didRead = true;

  // TODO: Should probably break this up on a thread
  var data = "";
  for (var i = 0; i < files.length; ++i) {
    G_Debug(this, "Trying to read: " + files[i].path);
    var gFile = new G_FileReader(files[i]);
    data += gFile.read() + "\n";
    gFile.close();
  }

  this.deserialize_(data, BindToObject(this.dataFromDisk, this));
  return true;
}

/**
 * Creates a WireFormatReader and calls deserialize on it.
 */
PROT_ListManager.prototype.deserialize_ = function(data, callback) {
  var wfr = new PROT_WireFormatReader(this.threadQueue_, this.tablesData);
  wfr.deserialize(data, callback);
}

/**
 * A callback that is executed when we have read our table data from
 * disk.
 *
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 */
PROT_ListManager.prototype.dataFromDisk = function(tablesKnown, tablesData) {
  G_Debug(this, "Called dataFromDisk");

  this.importData_(tablesKnown, tablesData);

  this.readingData_ = false;

  // If we have more files to read schedule another round of reading
  var files = this.listUnreadDataFiles();
  if (files.length) 
    new G_Alarm(BindToObject(this.readDataFiles, this), 0);

  return true;
}

/**
 * Prepares a URL to fetch upates from. Format is a squence of 
 * type:major:minor, fields
 * 
 * @param url The base URL to which query parameters are appended; assumes
 *            already has a trailing ?
 * @returns the URL that we should request the table update from.
 */
PROT_ListManager.prototype.getRequestURL_ = function(url) {
  url += "version=";
  var firstElement = true;
  var requestMac = false;

  for(var type in this.tablesKnown_) {
    // All tables with a major of 0 are internal tables that we never
    // update remotely.
    if (this.tablesKnown_[type].major == 0)
      continue;

    // Check if the table needs updating
    if (this.tablesKnown_[type].needsUpdate == false)
      continue;

    if (!firstElement) {
      url += ","
    } else {
      firstElement = false;
    }
    url += type + ":" + this.tablesKnown_[type].toUrl();

    if (this.tablesKnown_[type].requireMac)
      requestMac = true;
  }

  // Request a mac only if at least one of the tables to be updated requires
  // it
  if (requestMac) {
    // Add the wrapped key for requesting macs
    if (!this.urlCrypto_)
      this.urlCrypto_ = new PROT_UrlCrypto();

    url += "&wrkey=" +
      encodeURIComponent(this.urlCrypto_.getManager().getWrappedKey());
  }

  G_Debug(this, "getRequestURL returning: " + url);
  return url;
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

  if (this.rpcPending_) {
    G_Debug(this, 'checkForUpdates: old callback is still pending...');
    return false;
  }

  if (this.readingData_) {
    G_Debug(this, 'checkForUpdate: still reading data from disk...');

    // Reschedule the update to happen in a little while
    new G_Alarm(BindToObject(this.checkForUpdates, this), 500);
    return false;
  }

  G_Debug(this, 'checkForUpdates: scheduling request..');
  this.rpcPending_ = true;
  this.xmlFetcher_ = new PROT_XMLFetcher();
  this.xmlFetcher_.get(this.getRequestURL_(this.updateserverURL_),
                       BindToObject(this.rpcDone, this));
  return true;
}

/**
 * A callback that is executed when the XMLHTTP request is finished
 *
 * @param data String containing the returned data
 */
PROT_ListManager.prototype.rpcDone = function(data) {
  G_Debug(this, "Called rpcDone");
  /* Runs in a thread and executes the callback when ready */
  this.deserialize_(data, BindToObject(this.dataReady, this));

  this.xmlFetcher_ = null;
}

/**
 * @returns Boolean indicating if it has read data from somewhere (e.g.,
 *          disk)
 */
PROT_ListManager.prototype.hasData = function() {
  return !!this.dataImportedAtLeastOnce_;
}

/**
 * Check if the mac passed, if required.
 *
 * @param oldParser The current version parser for the table
 * @param newParser A version parser returned by the WireFormatReader
 *
 * @returns true if no mac is required, or if it is and the newParser says it
 * didn't fail, false otherwise
 */
PROT_ListManager.prototype.maybeCheckMac = function(oldParser, newParser) {
  if (!oldParser.requireMac)
    return true;

  if (newParser.mac && !newParser.macFailed)
    return true;

  G_Debug(this, "mac required and it failed");
  return false;
}

/**
 * We've deserialized tables from disk or the update server, now let's
 * swap them into the tables we're currently using.
 * 
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 * @returns Array of strings holding the names of tables we updated
 */
PROT_ListManager.prototype.importData_ = function(tablesKnown, tablesData) {
  this.dataImportedAtLeastOnce_ = true;

  var changes = [];

  // If our data has changed, update it
  if (tablesKnown && tablesData) {
    // Update our tables with the new data
    for (var name in tablesKnown) {
      // WireFormatReader constructs VersionParsers from scratch and doesn't
      // know about the requireMac flag, so check it now
      if (tablesKnown[name] && this.tablesKnown_[name] &&
          this.tablesKnown_[name] != tablesKnown[name] &&
          this.maybeCheckMac(this.tablesKnown_[name], tablesKnown[name])) {
        changes.push(name);

        this.tablesKnown_[name].ImportVersion(tablesKnown[name]);
        this.tablesData[name] = tablesData[name];
      }
    }
  }

  return changes;
}

/**
 * A callback that is executed when we have updated the tables from
 * the server. We are provided with the new table versions and the
 * corresponding data.
 *
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 */
PROT_ListManager.prototype.dataReady = function(tablesKnown, tablesData) {
  G_Debug(this, "Called dataReady");

  // First, replace the current tables we're using
  var changes = this.importData_(tablesKnown, tablesData);

  // Then serialize the new tables to disk
  if (changes.length) {
    G_Debug(this, "Committing " + changes.length + " changed tables to disk.");
    for (var i = 0; i < changes.length; i++) {
      this.storeTable(changes[i], 
                      this.tablesData[changes[i]], 
                      this.tablesKnown_[changes[i]]);
    }
  }

  this.rpcPending_ = false;            // todo maybe can do away cuz asynch
  G_Debug(this, "Done writing data to disk.");
}

/**
 * Serialize a table to disk.
 *
 * @param tableName String holding the name of the table to serialize
 * 
 * @param opt_table Reference to the Map holding the table (if omitted,
 *                  we look the table up)
 *
 * @param opt_parser Reference to the versionparser for this table (if
 *                   omitted we look the table up)
 */
PROT_ListManager.prototype.storeTable = function(tableName, 
                                                 opt_table, 
                                                 opt_parser) {

  var table = opt_table ? opt_table : this.tablesData[tableName];
  var parser = opt_parser ? opt_parser : this.tablesKnown_[tableName];
 
  if (!table || ! parser) 
    G_Error(this, "Tried to serialize a non-existent table: " + tableName);

  var wfw = new PROT_WireFormatWriter(this.threadQueue_);
  wfw.serialize(table, parser, BindToObject(this.writeDataFile, 
                                            this, 
                                            tableName));
}

/**
 * Takes a serialized table and writes it into our application directory.
 * 
 * @param tableName String containing the name of the table, used to
 *                  create the filename
 *
 * @param tableData Serialized version of the table
 */
PROT_ListManager.prototype.writeDataFile = function(tableName, tableData) {
  var filename = tableName + ".sst";

  G_Debug(this, "Serializing to " + filename);

  try {
    var tmpFile = G_File.createUniqueTempFile(filename);
    var tmpFileWriter = new G_FileWriter(tmpFile);
    
    tmpFileWriter.write(tableData);
    tmpFileWriter.close();

  } catch(e) {
    G_Debug(this, e);
    G_Error(this, "Couldn't write to temp file: " + filename);
  }

  // Now overwrite!
  try {
    tmpFile.moveTo(this.appDir_, filename);
  } catch(e) {
    G_Debug(this, e);
    G_Error(this, "Couldn't overwrite existing table: " +
            tmpFile.path + ', ' +
            this.appDir_.path + ', ' + filename);
    tmpFile.remove(false /* not recursive */);
  }
  G_Debug(this, "Serializing to " + filename + " finished.");
}

PROT_ListManager.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsISample) ||
      iid.equals(Components.interfaces.nsIUrlListManager))
    return this;

  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;
}

// A simple factory function that creates nsIUrlClassifierTable instances based
// on a name.  The name is a string of the format
// provider_name-semantic_type-table_type.  For example, goog-white-enchash
// or goog-black-url.
function newUrlClassifierTable(name) {
  G_Debug("protfactory", "Trying to create a new nsIUrlClassifierTable: " + name);
  var tokens = name.split('-');
  var type = tokens[2];
  var table = Cc['@mozilla.org/url-classifier/table;1?type=' + type]
                .createInstance(Ci.nsIUrlClassifierTable);
  table.name = name;
  return table;
}
