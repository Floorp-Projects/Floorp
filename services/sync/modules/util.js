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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['Utils'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/log4moz.js");

/*
 * Utility functions
 */

let Utils = {

  deepEquals: function Weave_deepEquals(a, b) {
    if (!a && !b)
      return true;
    if (!a || !b)
      return false;

    // if neither is an object, just use ==
    if (typeof(a) != "object" && typeof(b) != "object")
      return a == b;

    // check if only one of them is an object
    if (typeof(a) != "object" || typeof(b) != "object")
      return false;

    if (a instanceof Array)
      dump("a is an array\n");
    if (b instanceof Array)
      dump("b is an array\n");

    if (a instanceof Array && b instanceof Array) {
      if (a.length != b.length)
        return false;

      for (let i = 0; i < a.length; i++) {
        if (!Utils.deepEquals(a[i], b[i]))
          return false;
      }

    } else {
      // check if only one of them is an array
      if (a instanceof Array || b instanceof Array)
        return false;

      for (let key in a) {
        if (!Utils.deepEquals(a[key], b[key]))
          return false;
      }
    }

    return true;
  },

  deepCopy: function Weave_deepCopy(thing) {
    if (typeof(thing) != "object" || thing == null)
      return thing;
    let ret;

    if (thing instanceof Array) {
      dump("making a cipy of an array!\n\n");
      ret = [];
      for (let i = 0; i < thing.length; i++)
        ret.push(Utils.deepCopy(thing[i]));

    } else {
      ret = {};
      for (let key in thing)
        ret[key] = Utils.deepCopy(thing[key]);
    }

    return ret;
  },

  checkStatus: function Weave_checkStatus(code, msg, ranges) {
    if (!ranges)
      ranges = [[200,300]];

    for (let i = 0; i < ranges.length; i++) {
      rng = ranges[i];
      if (typeof(rng) == "object" && code >= rng[0] && code < rng[1])
        return;
      else if (typeof(rng) == "integer" && code == rng)
        return;
    }

    let log = Log4Moz.Service.getLogger("Service.Util");
    log.error(msg + " Error code: " + code);
    throw 'checkStatus failed';
  },

  makeURI: function Weave_makeURI(URIString) {
    if (URIString === null || URIString == "")
      return null;
    let ioservice = Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
    return ioservice.newURI(URIString, null, null);
  },

  xpath: function Weave_xpath(xmlDoc, xpathString) {
    let root = xmlDoc.ownerDocument == null ?
      xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement
    let nsResolver = xmlDoc.createNSResolver(root);

    return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                           Ci.nsIDOMXPathResult.ANY_TYPE, null);
  },

  bind2: function Weave_bind2(object, method) {
    return function innerBind() { return method.apply(object, arguments); }
  },

  // Meant to be used like this in code that imports this file:
  //
  // Function.prototype.async = generatorAsync;
  //
  // So that you can do:
  //
  // gen = fooGen.async(...);
  // ret = yield;
  //
  // where fooGen is a generator function, and gen is the running generator.
  // ret is whatever the generator 'returns' via generatorDone().

  generatorAsync: function Weave_generatorAsync(self, extra_args) {
    try {
      let args = Array.prototype.slice.call(arguments, 1);
      let gen = this.apply(self, args);
      gen.next(); // must initialize before sending
      gen.send([gen, function(data) {Utils.continueGenerator(gen, data);}]);
      return gen;
    } catch (e) {
      if (e instanceof StopIteration) {
        dump("async warning: generator stopped unexpectedly");
        return null;
      } else {
        dump("Exception caught: " + e.message);
      }
    }
  },

  continueGenerator: function Weave_continueGenerator(generator, data) {
    try { generator.send(data); }
    catch (e) {
      if (e instanceof StopIteration)
        dump("continueGenerator warning: generator stopped unexpectedly");
      else
        dump("Exception caught: " + e.message);
    }
  },

  // generators created using Function.async can't simply call the
  // callback with the return value, since that would cause the calling
  // function to end up running (after the yield) from inside the
  // generator.  Instead, generators can call this method which sets up
  // a timer to call the callback from a timer (and cleans up the timer
  // to avoid leaks).  It also closes generators after the timeout, to
  // keep things clean.
  generatorDone: function Weave_generatorDone(object, generator, callback, retval) {
    if (object._timer)
      throw "Called generatorDone when there is a timer already set."

    let cb = Utils.bind2(object, function(event) {
      generator.close();
      generator = null;
      object._timer = null;
      if (callback)
        callback(retval);
    });

    object._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    object._timer.initWithCallback(new Utils.EventListener(cb),
                                   0, object._timer.TYPE_ONE_SHOT);
  },

  runCmd: function Weave_runCmd() {
    var binary;
    var args = [];

    for (let i = 0; i < arguments.length; ++i) {
      args.push(arguments[i]);
    }

    if (args[0] instanceof Ci.nsIFile) {
      binary = args.shift();
    } else {
      binary = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      binary.initWithPath(args.shift());
    }

    var p = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    p.init(binary);

    let log = Log4Moz.Service.getLogger("Service.Util");
    log.debug("Running command: " + binary.path + " " + args.join(" "));

    p.run(true, args, args.length);
    return p.exitValue;
  },

  getTmp: function Weave_getTmp(name) {
    let ds = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);

    let tmp = ds.get("ProfD", Ci.nsIFile);
    tmp.QueryInterface(Ci.nsILocalFile);

    tmp.append("weave");
    tmp.append("tmp");
    if (!tmp.exists())
      tmp.create(tmp.DIRECTORY_TYPE, PERMS_DIRECTORY);

    if (name)
      tmp.append(name);

    return tmp;
  },
  
  open: function open(pathOrFile, mode, perms) {
    let stream, file;

    if (pathOrFile instanceof Ci.nsIFile) {
      file = pathOrFile;
    } else {
      file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      dump("PATH IS" + pathOrFile + "\n");
      file.initWithPath(pathOrFile);
    }

    if (!perms)
      perms = PERMS_FILE;

    switch(mode) {
    case "<": {
      if (!file.exists())
        throw "Cannot open file for reading, file does not exist";
      let fis = Cc["@mozilla.org/network/file-input-stream;1"].
        createInstance(Ci.nsIFileInputStream);
      fis.init(file, MODE_RDONLY, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-input-stream;1"].
        createInstance(Ci.nsIConverterInputStream);
      stream.init(fis, "UTF-8", 4096,
                  Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
    } break;

    case ">": {
      let fos = Cc["@mozilla.org/network/file-output-stream;1"].
        createInstance(Ci.nsIFileOutputStream);
      fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-output-stream;1"]
        .createInstance(Ci.nsIConverterOutputStream);
      stream.init(fos, "UTF-8", 4096, 0x0000);
    } break;

    case ">>": {
      let fos = Cc["@mozilla.org/network/file-output-stream;1"].
        createInstance(Ci.nsIFileOutputStream);
      fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_APPEND, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-output-stream;1"]
        .createInstance(Ci.nsIConverterOutputStream);
      stream.init(fos, "UTF-8", 4096, 0x0000);
    } break;

    default:
      throw "Illegal mode to open(): " + mode;
    }

    return [stream, file];
  },

  // assumes an nsIConverterInputStream
  readStream: function Weave_readStream(is) {
    let ret = "", str = {};
    while (is.readString(4096, str) != 0) {
      ret += str.value;
    }
    return ret;
  },

  /*
   * Event listener object
   * Used to handle XMLHttpRequest and nsITimer callbacks
   */

  EventListener: function Weave_EventListener(handler, eventName) {
    this._handler = handler;
    this._eventName = eventName;
    this._log = Log4Moz.Service.getLogger("Service.EventHandler");
  }
};

Utils.EventListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsISupports]),

  // DOM event listener
  handleEvent: function EL_handleEvent(event) {
    this._log.trace("Handling event " + this._eventName);
    this._handler(event);
  },

  // nsITimerCallback
  notify: function EL_notify(timer) {
    this._log.trace("Timer fired");
    this._handler(timer);
  }
}
