/**
 * MozFileLogger, a log listener that can write to a local file.
 */

var ipcMode = false; // running in e10s build and need to use IPC?

function contentDispatchEvent(type, data, sync) {
  if (typeof(data) === "undefined") {
    data = {};
  }

  var element = document.createEvent("datacontainerevent");
  element.initEvent("contentEvent", true, false);
  element.setData("sync", sync);
  element.setData("type", type);
  element.setData("data", JSON.stringify(data));
  document.dispatchEvent(element);
}

function contentSyncEvent(type, data) {
  contentDispatchEvent(type, data, 1);
}

function contentAsyncEvent(type, data) {
  contentDispatchEvent(type, data, 0);
}

//double logging to account for normal mode and ipc mode (mobile_profile only)
//Ideally we would remove the dump() and just do ipc logging
function dumpLog(msg) {
  dump(msg);
  MozFileLogger.log(msg);
}


netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

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
var MozFileLogger = {};


MozFileLogger.init = function(path) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  MozFileLogger._file = Cc[LF_CID].createInstance(Ci.nsILocalFile);
  MozFileLogger._file.initWithPath(path);
  MozFileLogger._foStream = Cc[FOSTREAM_CID].createInstance(Ci.nsIFileOutputStream);
  MozFileLogger._foStream.init(this._file, PR_WRITE_ONLY | PR_CREATE_FILE | PR_APPEND,
                                   0o664, 0);
}

MozFileLogger.getLogCallback = function() {
  return function (msg) {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    var data = msg.num + " " + msg.level + " " + msg.info.join(' ') + "\n";
    if (MozFileLogger._foStream)
      MozFileLogger._foStream.write(data, data.length);

    if (data.indexOf("SimpleTest FINISH") >= 0) {
      MozFileLogger.close();
    }
  }
}

// This is only used from chrome space by the reftest harness
MozFileLogger.log = function(msg) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  try {
    if (MozFileLogger._foStream)
      MozFileLogger._foStream.write(msg, msg.length);
  } catch(ex) {}
}

MozFileLogger.close = function() {
  if (ipcMode) {
    contentAsyncEvent("LoggerClose");
    return;
  }

  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  if(MozFileLogger._foStream)
    MozFileLogger._foStream.close();
  
  MozFileLogger._foStream = null;
  MozFileLogger._file = null;
}

try {
  var prefs = Components.classes['@mozilla.org/preferences-service;1']
    .getService(Components.interfaces.nsIPrefBranch2);
  var filename = prefs.getCharPref('talos.logfile');
  MozFileLogger.init(filename);
} catch (ex) {} //pref does not exist, return empty string

