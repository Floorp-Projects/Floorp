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


// A class that serializes and deserializes opaque key/value string to
// string maps to/from maps (trtables). It knows how to create
// trtables from the serialized format, so it also understands
// meta-information like the name of the table and the table's
// version. See docs for the protocol description.
// 
// TODO: wireformatreader: if you have multiple updates for one table
//       in a call to deserialize, the later ones will be merged 
//       (all but the last will be ignored). To fix, merge instead
//       of replace when you have an existing table, and only do so once.
// TODO must have blank line between successive types -- problem?
// TODO doesn't tolerate blank lines very well
//
// Maybe: These classes could use a LOT more cleanup, but it's not a
//       priority at the moment. For example, the tablesData/Known
//       maps should be combined into a single object, the parser
//       for a given type should be separate from the version info,
//       and there should be synchronous interfaces for testing.


/**
 * A class that knows how to serialize and deserialize meta-information.
 * This meta information is the table name and version number, and 
 * in its serialized form looks like the first line below:
 * 
 * [name-of-table X.Y update?]                
 * ...key/value pairs to add or delete follow...
 * <blank line ends the table>
 *
 * The X.Y is the version number and the optional "update" token means 
 * that the table is a differential from the curent table the extension
 * has. Its absence means that this is a full, new table.
 */
function PROT_VersionParser(type, opt_major, opt_minor, opt_requireMac) {
  this.debugZone = "versionparser";
  this.type = type;
  this.major = 0;
  this.minor = 0;

  this.badHeader = false;

  // Should the wireformatreader compute a mac?
  this.mac = false;
  this.macval = "";
  this.macFailed = false;
  this.requireMac = !!opt_requireMac;

  this.update = false;
  // Used by ListerManager to see if we have read data for this table from
  // disk.  Once we read a table from disk, we are not going to do so again
  // but instead update remotely if necessary.
  this.didRead = false;
  if (opt_major)
    this.major = parseInt(opt_major);
  if (opt_minor)
    this.minor = parseInt(opt_minor);
}

/** Import the version information from another VersionParser
 * @params version a version parser object
 */
PROT_VersionParser.prototype.ImportVersion = function(version) {
  this.major = version.major;
  this.minor = version.minor;

  this.mac = version.mac;
  this.macFailed = version.macFailed;
  this.macval = version.macval;
  // Don't set requireMac, since wfr creates vparsers from scratch and doesn't
  // know about it
}

/** 
 * Creates a string like [goog-white-black 1.1] from internal information
 * 
 * @returns version string
 */
PROT_VersionParser.prototype.toString = function() {
  var s = "[" + this.type + " " + this.major + "." + this.minor + "]";
  if (this.mac)
    s += "[mac=" + this.macval + "]";
  return s;
}

/** 
 * Creates a string like 1:1 from internal information used for
 * fetching updates from the server. Called by the listmanager.
 * 
 * @returns version string
 */
PROT_VersionParser.prototype.toUrl = function() {
  return this.major + ":" + this.minor;
}

/**
 * Process the old format, [type major.minor [update]]
 *
 * @returns true if the string could be parsed, false otherwise
 */
PROT_VersionParser.prototype.processOldFormat_ = function(line) {
  if (line[0] != '[' || line.slice(-1) != ']')
    return false;

  var description = line.slice(1, -1);

  // Get the type name and version number of this table
  var tokens = description.split(" ");
  this.type = tokens[0];
  var majorminor = tokens[1].split(".");
  this.major = parseInt(majorminor[0]);
  this.minor = parseInt(majorminor[1]);
  if (isNaN(this.major) || isNaN(this.minor))
    return false;

  if (tokens.length >= 3) {
     this.update = tokens[2] == "update";
  }

  return true;
}

/** 
 * Takes a string like [name-of-table 1.1] and figures out the type
 * and corresponding version numbers.
 * 
 * @returns true if the string could be parsed, false otherwise
 */
PROT_VersionParser.prototype.fromString = function(line) {
  G_Debug(this, "Calling fromString with line: " + line);
  if (line[0] != '[' || line.slice(-1) != ']')
    return false;

  // There could be two [][], so take care of it
  var secondBracket = line.indexOf('[', 1);
  var firstPart = null;
  var secondPart = null;

  if (secondBracket != -1) {
    firstPart = line.substring(0, secondBracket);
    secondPart = line.substring(secondBracket);
    G_Debug(this, "First part: " + firstPart + " Second part: " + secondPart);
  } else {
    firstPart = line;
    G_Debug(this, "Old format: " + firstPart);
  }

  if (!this.processOldFormat_(firstPart))
    return false;

  if (secondPart && !this.processOptTokens_(secondPart))
    return false;

  return true;
}

/**
 * Process optional tokens
 *
 * @param line A string [token1=val1 token2=val2...]
 * @returns true if the string could be parsed, false otherwise
 */
PROT_VersionParser.prototype.processOptTokens_ = function(line) {
  if (line[0] != '[' || line.slice(-1) != ']')
    return false;
  var description = line.slice(1, -1);
  // Get the type name and version number of this table
  var tokens = description.split(" ");

  for (var i = 0; i < tokens.length; i++) {
    G_Debug(this, "Processing optional token: " + tokens[i]);
    var tokenparts = tokens[i].split("=");
    switch(tokenparts[0]){
    case "mac":
      this.mac = true;
      if (tokenparts.length < 2) {
        G_Debug(this, "Found mac flag but not mac value!");
	return false;
      }
      // The mac value may have "=" in it, so we can't just use tokenparts[1].
      // Instead, just take the rest of tokens[i] after the first "="
      this.macval = tokens[i].substr(tokens[i].indexOf("=")+1);
      break;
    default:
      G_Debug(this, "Found unrecognized token: " + tokenparts[0]);
      break;
    }
  }

  return true;
}


/**
 * A WireFormatWriter can serialize table data
 *
 * @param threadQueue A thread queue we should run on
 *
 * @constructor
 */
function PROT_WireFormatWriter(threadQueue) {
  this.threadQueue_ = threadQueue;
  this.debugZone = "wireformatwriter";
}

/**
 * Serializes a table to a string.
 *
 * @param tableData Reference to the table data we should serialize
 *
 * @param vParser Reference to the version parser/unparser we should use
 *
 * @param callback Reference to a function we should call when complete.
 *                 The callback will be invoked with a string holding 
 *                 the serialized data as an argument
 *
 * @returns False if the serializer is busy, else true
 *
 */
PROT_WireFormatWriter.prototype.serialize = function(tableData,
                                                     vParser, 
                                                     callback) {
  if (this.callback_) {
    G_Debug(this, "Serializer busy");
    return false;
  }

  this.callback_ = callback;
  this.current_ = 0;
  this.serialized_ = vParser.toString() + "\n";
  var cnt = {};
  this.keyList_ = tableData.getKeys(cnt);
  this.tableData_ = tableData;

  this.threadQueue_.addWorker(BindToObject(this.doWorkUnit, this),
                              BindToObject(this.isComplete, this));
  this.threadQueue_.run();

  return true;
}

/**
 * Serialize a chunk of data. This is periodically invoked by the
 * threadqueue.
 */
PROT_WireFormatWriter.prototype.doWorkUnit = function() {
  for (var i = 0; i < 10 && this.current_ < this.keyList_.length; i++) {
    var key = this.keyList_[this.current_];
    if (key) {
      var value = this.tableData_.findValue(key);
      this.serialized_ += "+" + key + "\t" + value + "\n";
    }
    this.current_++;
  }

  if (this.isComplete()) 
    this.onComplete();
}

/**
 * Are we done serializing? Called by the threadqueue to tell when
 * to stop running this thread.
 *
 * @returns Boolean indicating if we're done serializing
 */
PROT_WireFormatWriter.prototype.isComplete = function() {
  return this.current_ >= this.keyList_.length;
}

/**
 * When we're done, call the callback
 */
PROT_WireFormatWriter.prototype.onComplete = function() {
  this.callback_(this.serialized_);
  this.callback_ = null;
}

/**
 * A WireFormatReader can deserialize data.
 *
 * @constructor
 *
 * @param threadQueue A global thread queue that we use to schedule our
 *        work so that we do not take up too much time at one.
 *
 * @param opt_existingTablesData Optional reference to a map of tables
 *         into which we should merge the data being deserialized 
 */
function PROT_WireFormatReader(threadQueue, opt_existingTablesData) {
  this.debugZone = "wireformatreader";
  this.existingTablesData_ = opt_existingTablesData;
  this.threadQueue_ = threadQueue;
  this.callback_ = null;

  // For mac'ing updates
  this.urlCrypto_ = null;
  var keyUrl = null;
  try {
    keyUrl = PROT_GlobalStore.getGetKeyURL();
  } catch (e) {
    G_Debug(this, "No key url, disabling mac'ed updates");
  }
  if (keyUrl) {
    this.urlCrypto_ = new PROT_UrlCrypto();
  }
}

/**
 * Starts a thread to process the input data.
 *
 * @param tableUpdate A string that contains the data for updating the
 *        tables that we know about.
 *
 * @param callback A user specificed callback that is being executed
 *        when data processing completes.  The callback is provided
 *        with two arguments: tablesKnown and tablesData
 *
 * @returns true is new data is being process, or false on failure.
 */
PROT_WireFormatReader.prototype.deserialize = function(tableUpdate, callback) {
  this.tableUpdate_ = tableUpdate;
  this.tablesKnown_ = {};             // version parsers
  this.tablesData_ = {};              // TRTables

  if (this.callback_ != null) {
    G_Debug(this, "previous deserialize is still running");
    return false;
  }

  this.callback_ = callback;
  this.offset_ = 0;
  this.resetTableState_();

  // On empty data, we just invoke the callback directly
  if (!this.tableUpdate_ || !this.tableUpdate_.length) {
    G_Debug(this, "No data. Calling back.");
    this.onComplete();
    return true;
  }

  this.threadQueue_.addWorker(BindToObject(this.processLine_, this),
                              BindToObject(this.isComplete, this));
  this.threadQueue_.run();

  return true;
}

/**
 * Resets the per table state
 */
PROT_WireFormatReader.prototype.resetTableState_ = function() {
  this.vParser_ = null;
  this.insert_ = 0;
  this.remove_ = 0;
}

/**
 * Initalizes our state to process a new table.
 * NOTE: For performance reasons, we might have drive the replacement of
 * the table via a thread.
 *
 * @param line The input line that contains the table name and version
 */
PROT_WireFormatReader.prototype.processNewTable_ = function(line) {
  this.vParser_ = new PROT_VersionParser("something");

  // If there's an error, give up
  if (!this.vParser_.fromString(line)) {
    G_Debug(this, "Error in table header, skipping");
    this.vParser_ = null;
    return;
  } 

  G_Debug(this, "processNewTable: " + this.vParser_.type + ": " +
          this.vParser_.major + ":" + this.vParser_.minor + " update=" +
          this.vParser_.update + " mac=" + this.vParser_.mac);
       
  // Create temporary table.  PROT_TRTable() blows up if it doesn't like its
  // name, so mask and just set vParser to null
  try {
    this.tablesData_[this.vParser_.type] =
                            newProtectionTable(this.vParser_.type);
  } catch(e) {
    G_Debug(this, 
            "Unable to initialize new TRTable, because of exception: " + e);
    this.vParser_ = null;
    return;
  }
  if (this.vParser_.update && this.existingTablesData_ && 
      this.existingTablesData_[this.vParser_.type]) {
    // If we update an existing table, we need to copy the old table
    // data from the pre-existing tables
    this.tablesData_[this.vParser_.type].replace(
        this.existingTablesData_[this.vParser_.type]);
  }
  G_Debug(this, "processNewTable done");
}

/**
 * Updates the contents of our temporary table. Exceptions should not 
 * be masked here -- they're caught at a higher level.
 *
 * @param line The input line that contains the new data
 */
PROT_WireFormatReader.prototype.processUpdateTable_ = function(line) {
  // Regular update to the current version
  var tokens = line.split("\t");
  var operation = tokens[0][0];
  var key = tokens[0].slice(1);
  var value = tokens[1];

  if (operation == "+") {
    this.tablesData_[this.vParser_.type].insert(key, value);
    this.insert_++;
  } else {
    this.tablesData_[this.vParser_.type].erase(key);
    this.remove_++;
  }
}

/**
 * Finish processing a table
 */
PROT_WireFormatReader.prototype.endTable_ = function() {
  G_Debug(this,
          "Finished: " + this.vParser_.type + " +" + 
          this.insert_ + " -" + this.remove_ + " mac'ing: " + this.vParser_.mac);
      
  if (this.vParser_.mac) {
    var mac = this.urlCrypto_.finishMac();
    if (mac != this.vParser_.macval) {
      G_Debug(this, "Mac didn't match! (" + mac + " != "
              + this.vParser_.macval + ")");
      this.vParser_.macFailed = true;
    } else {
      G_Debug(this, "Computed mac matched sent mac: " + this.vParser_.macval);
    }
  }

  // Rollback if the mac failed.
  if (this.vParser_.mac && this.vParser_.macFailed) {
    this.tablesData_[this.vParser_.type] = undefined;
    this.tablesKnown_[this.vParser_.type] = undefined;
    G_Debug(this, "throwing away " + this.vParser_.type);
  } else {
    this.tablesKnown_[this.vParser_.type] = this.vParser_;
  }

  this.resetTableState_();
}

/**
 * Processes a chunk of data.  TODO(MC): Make it so the mac is computed from a
 * saved begin and end offset instead of computing line by line.
 */
PROT_WireFormatReader.prototype.processLine_ = function() {
  //G_Debug(this, '> processline');
  for (var count = 0;
       count < 5 && this.offset_ < this.tableUpdate_.length; count++) {
    var newOffset = this.tableUpdate_.indexOf("\n", this.offset_);
    var line = "";
    if (newOffset == -1) {
      this.offset_ = this.tableUpdate_.length;
    } else {
      line = this.tableUpdate_.slice(this.offset_, newOffset);
      this.offset_ = newOffset + 1;
    }

    // Ignore empty lines if we currently do not have a table
    if (!this.vParser_ && (!line || line[0] != '[')) {
      continue;
    }

    if (!line) {
      // End of one table - pop the results
      this.endTable_();
    } else if (line[0] == '[' && line.slice(-1) == ']') {
      // processNewTable doesn't instantiate this.vParser_ in case of malformed
      // headers, so the rest of the table lines will get skipped
      this.processNewTable_(line);

      if (this.vParser_ && this.vParser_.mac) {
        this.urlCrypto_.initMac();
      }
    } else {
      if (!this.vParser_) {
        G_Debug(this, "Ignoring: " + line);
        continue;
      }

      // Now we try to read a data line. However the table could've
      // been corrupted on disk (e.g., the browser suddenly quit while
      // we were in the middle of writing a line). If so,
      // porcessUpdateTable() will throw. In this case we want to
      // resynch the whole table, so we skip this line and then
      // explicitly set the table's minor version to -1 (the lowest
      // possible -- not 0, which means the table is local), causing a
      // full update the next time we ask for it.
      //
      // We ignore the case where we wrote an incomplete but malformed
      // table -- it fixes itself over time as the missing keys become
      // less relevant.
      
      try {
        this.processUpdateTable_(line);
        // Compute the mac over all the next lines until we finish the table
        // TODO: This is ugly!  line doesn't get the final '\n', so add it back.
        // We could save the original line back up top, and use that instead,
        // but that's yet more copying...
        if (this.vParser_.mac) {
          this.urlCrypto_.updateMacFromString(line + "\n");
        }
      } catch(e) {
        G_Debug(this, "MALFORMED TABLE LINE: [" + line + "]\n" +
                "Skipping this line, and resetting table " + 
                this.vParser_.type + " to version -1.\n" +
                "(This as a result of exception: " + e + ")");
        this.vParser_.minor = "-1";
      }
    }
  }

  // If the table we're reading is the last, then the for loop will
  // fail, causing the table finish logic to be skipped. So here 
  // ensure that we finish up whatever table we're working on.

  if (this.vParser_ && this.offset_ >= this.tableUpdate_.length) {
    G_Debug(this,
            "Finished (final table): " + this.vParser_.type + " +" + 
            this.insert_ + " -" + this.remove_);
    this.endTable_();
  }

  if (this.isComplete()) this.onComplete();
  //G_Debug(this, "< processline");
}

/**
 * Returns true if all input data has been processed
 */
PROT_WireFormatReader.prototype.isComplete = function() {
  return this.offset_ >= this.tableUpdate_.length;
}

/**
 * Notifies our caller of completion.
 */
PROT_WireFormatReader.prototype.onComplete = function() {
  G_Debug(this, "Processing complete. Executing callback.");
  this.callback_(this.tablesKnown_, this.tablesData_);
  this.callback_ = null;
}


function TEST_PROT_WireFormat() {
  if (G_GDEBUG) {
    // Sorry, this is incredibly ugly. What we need is continuations -- each
    // unittest that passes invokes the next.

    var z = "wireformat UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    function testMalformedTables() {
      G_Debug(z, "Testing malformed tables...");
        
      var wfr = new PROT_WireFormatReader(new TH_ThreadQueue());
      
      // Damn these unittests are ugly. Ugh. Now test handling corrupt tables
      var data = 
        "[test1-black-url 1.1]\n" +
        "+foo1\tbar\n" +
        "+foo2\n" +           // Malformed
        "+foo3\tbar\n" +
        "+foo4\tbar\n" +
        "\n" +
        "[test2-black-url 1.2]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n" +
        "\n" +
        "[test3-black-url 1.3]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\n" +          // Malformed
        "\n" +
        "[test4-black-url 1.4]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n" + 
        "\n" +
        "[test4-badheader-url 1.asfd dfui]\n" + // Malformed header
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n";
        "\n" +
        "[test4-bad-name-url 1.asfd dfui]\n" + // Malformed header
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n";

      function malformedcb(tablesKnown, tablesData) {
        
        // Table has malformed data
        G_Assert(z, tablesKnown["test1-black-url"].minor == "-1", 
                 "test table 1 didn't get reset");
        G_Assert(z, !!tablesData["test1-black-url"].find("foo1"), 
                 "test table 1 didn't set keys before the error");
        G_Assert(z, !!tablesData["test1-black-url"].find("foo3"),
                 "test table 1 didn't set keys after the error");

        // Table should be good
        G_Assert(z, tablesKnown["test2-black-url"].minor == "2", 
                 "test table 1 didnt get correct version number");
        G_Assert(z, !!tablesData["test2-black-url"].find("foo4"), 
                 "test table 2 didnt parse properly");
        
        // Table is malformed
        G_Assert(z, tablesKnown["test3-black-url"].minor == "-1", 
                 "test table 3 didn't get reset");
        G_Assert(z, !tablesData["test3-black-url"].find("foo4"),
                 "test table 3 somehow has its malformed line?");
        
        // Table should be good
        G_Assert(z, tablesKnown["test4-black-url"].minor == "4", 
                 "test table 4 didn't get correct version number");
        G_Assert(z, !!tablesData["test2-black-url"].find("foo4"), 
                 "test table 4 didnt parse properly");

        // Table is malformed and should be unknown
        G_Assert(z, !tablesKnown["test4-badheader-url"],
                 "test table header should't be known");

        // Table is malformed and should be unknown
        G_Assert(z, !tablesKnown["test4-bad-name-url"],
                 "test table header should't be known");

        G_Debug(z, "PASSED");
      };
      
      wfr.deserialize(data, malformedcb);
    };

    var testTablesData = {};
    
    var tableName = "test-black-url";
    var data1 = "[" + tableName + " 1.5]\n";
    for (var i = 0; i < 100; i++)
      data1 += "+http://exists" + i + "\t1\n";
    data1 += "-http://exists50\t1\n";
    data1 += "-http://exists666\t1\n";

    var data2 = "[" + tableName + " 1.7 update]\n";
    for (var i = 0; i < 100; i++)
      data2 += "-http://exists" + i + "\t1\n";

    var set3Name = "test-white-domain";
    var set3data = "";
    // Use this string since it's the same in the update server unittest in
    // /firefox/security/scripts/test/cheese/cheesey something.sh
    for (var i = 1; i <= 3; i++) {
      set3data += "+white" + i + ".com\t1\n";
    }
    var urlCrypto = new PROT_UrlCrypto();
    var computedMac = urlCrypto.computeMac(set3data);

    function data1cb(tablesKnown, tablesData) {
      G_Assert(z, 
               tablesData[tableName] != null,
               "Didn't get our table back");

      for (var i = 0; i < 100; i++)
        if (i != 50)
          G_Assert(z, 
                   tablesData[tableName].find("http://exists" + i) == "1", 
                   "Item addition broken");

      G_Assert(z, 
               !tablesData[tableName].find("http://exists50"), 
               "Item removal broken");
      G_Assert(z, 
               !tablesData[tableName].find("http://exists666"), 
               "Non-existent item");

      G_Assert(z, tablesKnown[tableName].major == "1", "Major parsing broke");
      G_Assert(z, tablesKnown[tableName].minor == "5", "Major parsing broke");

      var wfr2 = new PROT_WireFormatReader(new TH_ThreadQueue(), tablesData);
      wfr2.deserialize(data2, data2cb);
    };

    function data2cb(tablesKnown, tablesData) {
      for (var i = 0; i < 100; i++)
        G_Assert(z, 
                 !tablesData[tableName].find("http://exists" + i),
                 "Tables merge broken");

      G_Assert(z, tablesKnown[tableName].major == "1", "Major parsing broke");
      G_Assert(z, tablesKnown[tableName].minor == "7", "Major parsing broke");

      var wfr3 = new PROT_WireFormatReader(new TH_ThreadQueue(), tablesData);
      var data3 = "[test-white-domain 1.1][mac=" + computedMac + "]\n" +
                  set3data;
      wfr3.deserialize(data3, data3cb);
    };

    // Test the MAC stuff
    function data3cb(tablesKnown, tablesData) {
      G_Assert(z, tablesData["test-white-domain"] != null,
               "Didn't get our table back though the mac was correct");
      
      // Now do the same thing with the wrong mac
      computedMac = "012" + computedMac.substr(3);
      var data4 = "[test-foo-domain 1.1][mac=" + computedMac + "]\n" +
                  set3data;
      var wfr4 = new PROT_WireFormatReader(new TH_ThreadQueue(), tablesData);
      wfr4.deserialize(data4, data4cb);
    }

    // Test the MAC stuff
    function data4cb(tablesKnown, tablesData) {
      G_Assert(z, !tablesKnown["test-foo-domain"],
               "Deserialized though our mac was wrong");
      
      G_Assert(z, !tablesData["test-foo-domain"],
               "Deserialized though our mac was wrong");

      testMalformedTables();

    }
    var wfr = new PROT_WireFormatReader(new TH_ThreadQueue());
    wfr.deserialize(data1, data1cb);
  }
}
