/**
 * MozillaFileLogger, a log listener that can write to a local file.
 */ 

//double logging to account for normal mode and ipc mode (mobile_profile only)
//Ideally we would remove the dump() and just do ipc logging
function dumpLog(msg) {
  dump(msg);
  MozillaFileLogger.log(msg);
}


if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
}

const FOSTREAM_CID = "@mozilla.org/network/file-output-stream;1";
const LF_CID = "@mozilla.org/file/local;1";

// File status flags. It is a bitwise OR of the following bit flags.
// Only one of the first three flags below may be used.
const PR_READ_ONLY    = 0x01; // Open for reading only.
const PR_WRITE_ONLY   = 0x02; // Open for writing only.
const PR_READ_WRITE   = 0x04; // Open for reading and writing.

// If the file does not exist, the file is created.
// If the file exists, this flag has no effect.
const PR_CREATE_FILE  = 0x08;

// The file pointer is set to the end of the file prior to each write.
const PR_APPEND       = 0x10;

// If the file exists, its length is truncated to 0.
const PR_TRUNCATE     = 0x20;

// If set, each write will wait for both the file data
// and file status to be physically updated.
const PR_SYNC         = 0x40;

// If the file does not exist, the file is created. If the file already
// exists, no action and NULL is returned.
const PR_EXCL         = 0x80;

/** Init the file logger with the absolute path to the file.
    It will create and append if the file already exists **/
var MozillaFileLogger = {};


MozillaFileLogger.init = function(path) {
  MozillaFileLogger._file = Cc[LF_CID].createInstance(Ci.nsILocalFile);
  MozillaFileLogger._file.initWithPath(path);
  MozillaFileLogger._foStream = Cc[FOSTREAM_CID].createInstance(Ci.nsIFileOutputStream);
  MozillaFileLogger._foStream.init(this._file, PR_WRITE_ONLY | PR_CREATE_FILE | PR_APPEND,
                                   0o664, 0);
}

MozillaFileLogger.getLogCallback = function() {
  return function (msg) {
    var data = msg.num + " " + msg.level + " " + msg.info.join(' ') + "\n";
    if (MozillaFileLogger._foStream)
      MozillaFileLogger._foStream.write(data, data.length);

    if (data.indexOf("SimpleTest FINISH") >= 0) {
      MozillaFileLogger.close();
    }
  }
}

// This is only used from chrome space by the reftest harness
MozillaFileLogger.log = function(msg) {
  try {
    if (MozillaFileLogger._foStream)
      MozillaFileLogger._foStream.write(msg, msg.length);
  } catch(ex) {}
}

MozillaFileLogger.close = function() {
  if(MozillaFileLogger._foStream)
    MozillaFileLogger._foStream.close();
  
  MozillaFileLogger._foStream = null;
  MozillaFileLogger._file = null;
}

try {
  var prefs = Cc['@mozilla.org/preferences-service;1']
    .getService(Ci.nsIPrefBranch2);
  var filename = prefs.getCharPref('talos.logfile');
  MozillaFileLogger.init(filename);
} catch (ex) {} //pref does not exist, return empty string

