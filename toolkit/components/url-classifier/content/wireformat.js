# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
  this.needsUpdate = false;  // used by ListManager to determine update policy
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
  // Don't set requireMac, since we create vparsers from scratch and doesn't
  // know about it
}

/** 
 * Creates a string like [goog-white-black 1.1] from internal information
 * 
 * @returns String
 */
PROT_VersionParser.prototype.toString = function() {
  var s = "[" + this.type + " " + this.major + "." + this.minor + "]";
  return s;
}

/**
 * Creates a string like 1.123 with the version number.  This is the
 * format we store in prefs.
 * @return String
 */
PROT_VersionParser.prototype.versionString = function() {
  return this.major + "." + this.minor;
}

/** 
 * Creates a string like 1:1 from internal information used for
 * fetching updates from the server. Called by the listmanager.
 * 
 * @returns String
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
 * Takes a string like [name-of-table 1.1 [update]][mac=MAC] and figures out the
 * type and corresponding version numbers.
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

#ifdef DEBUG
function TEST_PROT_WireFormat() {
  if (G_GDEBUG) {
    var z = "versionparser UNITTEST";
    G_Debug(z, "Starting");

    var vp = new PROT_VersionParser("dummy");
    G_Assert(z, vp.fromString("[foo-bar-url 1.234]"),
             "failed to parse old format");
    G_Assert(z, "foo-bar-url" == vp.type, "failed to parse type");
    G_Assert(z, "1" == vp.major, "failed to parse major");
    G_Assert(z, "234" == vp.minor, "failed to parse minor");

    vp = new PROT_VersionParser("dummy");
    G_Assert(z, vp.fromString("[foo-bar-url 1.234][mac=567]"),
             "failed to parse new format");
    G_Assert(z, "foo-bar-url" == vp.type, "failed to parse type");
    G_Assert(z, "1" == vp.major, "failed to parse major");
    G_Assert(z, "234" == vp.minor, "failed to parse minor");
    G_Assert(z, true == vp.mac, "failed to parse mac");
    G_Assert(z, "567" == vp.macval, "failed to parse macval");

    G_Debug(z, "PASSED");
  }
}
#endif
