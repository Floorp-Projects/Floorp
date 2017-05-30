/**
 * MozillaLogger, a base class logger that just logs to stdout.
 */

"use strict";

/* import-globals-from specialpowers.js */

function MozillaLogger(aPath) {
}

function formatLogMessage(msg) {
    return msg.info.join(" ") + "\n";
}

MozillaLogger.prototype = {
  init(path) {},

  getLogCallback() {
    return function(msg) {
      var data = formatLogMessage(msg);
      dump(data);
    };
  },

  log(msg) {
    dump(msg);
  },

  close() {}
};


/**
 * SpecialPowersLogger, inherits from MozillaLogger and utilizes SpecialPowers.
 * intented to be used in content scripts to write to a file
 */
function SpecialPowersLogger(aPath) {
  // Call the base constructor
  MozillaLogger.call(this);
  this.prototype = new MozillaLogger(aPath);
  this.init(aPath);
}

SpecialPowersLogger.prototype = {
  init(path) {
    SpecialPowers.setLogFile(path);
  },

  getLogCallback() {
    return function(msg) {
      var data = formatLogMessage(msg);
      SpecialPowers.log(data);

      if (data.indexOf("SimpleTest FINISH") >= 0) {
        SpecialPowers.closeLogFile();
      }
    };
  },

  log(msg) {
    SpecialPowers.log(msg);
  },

  close() {
    SpecialPowers.closeLogFile();
  }
};


/**
 * MozillaFileLogger, a log listener that can write to a local file.
 * intended to be run from chrome space
 */

/** Init the file logger with the absolute path to the file.
    It will create and append if the file already exists **/
function MozillaFileLogger(aPath) {
  // Call the base constructor
  MozillaLogger.call(this);
  this.prototype = new MozillaLogger(aPath);
  this.init(aPath);
}

MozillaFileLogger.prototype = {

  init(path) {
    var PR_WRITE_ONLY   = 0x02; // Open for writing only.
    var PR_CREATE_FILE  = 0x08;
    var PR_APPEND       = 0x10;
    this._file = Components.classes["@mozilla.org/file/local;1"].
                            createInstance(Components.interfaces.nsILocalFile);
    this._file.initWithPath(path);
    this._foStream = Components.classes["@mozilla.org/network/file-output-stream;1"].
                                     createInstance(Components.interfaces.nsIFileOutputStream);
    this._foStream.init(this._file, PR_WRITE_ONLY | PR_CREATE_FILE | PR_APPEND,
                                     436 /* 0664 */, 0);

    this._converter = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
                    createInstance(Components.interfaces.nsIConverterOutputStream);
    this._converter.init(this._foStream, "UTF-8", 0, 0);
  },

  getLogCallback() {
    return function(msg) {
      var data = formatLogMessage(msg);
      if (MozillaFileLogger._converter) {
        this._converter.writeString(data);
      }

      if (data.indexOf("SimpleTest FINISH") >= 0) {
        MozillaFileLogger.close();
      }
    };
  },

  log(msg) {
    if (this._converter) {
      this._converter.writeString(msg);
    }
  },
  close() {
    if (this._converter) {
      this._converter.flush();
      this._converter.close();
    }

    this._foStream = null;
    this._converter = null;
    this._file = null;
  }
};

this.MozillaLogger = MozillaLogger;
this.SpecialPowersLogger = SpecialPowersLogger;
this.MozillaFileLogger = MozillaFileLogger;
