/**
 * MozillaLogger, a base class logger that just logs to stdout.
 */

"use strict";

function formatLogMessage(msg) {
  return msg.info.join(" ") + "\n";
}

function importMJS(mjs) {
  if (typeof ChromeUtils === "object") {
    return ChromeUtils.importESModule(mjs);
  }
  /* globals SpecialPowers */
  return SpecialPowers.ChromeUtils.importESModule(mjs);
}

// When running in release builds, we get a fake Components object in
// web contexts, so we need to check that the Components object is sane,
// too, not just that it exists.
let haveComponents =
  typeof Components === "object" &&
  typeof Components.Constructor === "function";

let CC = (
  haveComponents ? Components : SpecialPowers.wrap(SpecialPowers.Components)
).Constructor;

let ConverterOutputStream = CC(
  "@mozilla.org/intl/converter-output-stream;1",
  "nsIConverterOutputStream",
  "init"
);

class MozillaLogger {
  get logCallback() {
    return msg => {
      this.log(formatLogMessage(msg));
    };
  }

  log(msg) {
    dump(msg);
  }

  close() {}
}

/**
 * MozillaFileLogger, a log listener that can write to a local file.
 * intended to be run from chrome space
 */

/** Init the file logger with the absolute path to the file.
    It will create and append if the file already exists **/
class MozillaFileLogger extends MozillaLogger {
  constructor(aPath) {
    super();

    const { FileUtils } = importMJS("resource://gre/modules/FileUtils.sys.mjs");

    this._file = FileUtils.File(aPath);
    this._foStream = FileUtils.openFileOutputStream(
      this._file,
      FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_APPEND
    );

    this._converter = ConverterOutputStream(this._foStream, "UTF-8");
  }

  get logCallback() {
    return msg => {
      if (this._converter) {
        var data = formatLogMessage(msg);
        this.log(data);

        if (data.includes("SimpleTest FINISH")) {
          this.close();
        }
      }
    };
  }

  log(msg) {
    if (this._converter) {
      this._converter.writeString(msg);
    }
  }

  close() {
    this._converter.flush();
    this._converter.close();

    this._foStream = null;
    this._converter = null;
    this._file = null;
  }
}

this.MozillaLogger = MozillaLogger;
this.MozillaFileLogger = MozillaFileLogger;
