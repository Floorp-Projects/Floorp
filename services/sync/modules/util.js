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

const EXPORTED_SYMBOLS = ['Utils', 'Svc'];

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
  // Generates a brand-new globally unique identifier (GUID).
  makeGUID: function makeGUID() {
    let uuidgen = Cc["@mozilla.org/uuid-generator;1"].
                  getService(Ci.nsIUUIDGenerator);
    return uuidgen.generateUUID().toString().replace(/[{}]/g, '');
  },

  // Returns a nsILocalFile representing a file relative to the
  // current user's profile directory.  If the argument is a string,
  // it should be a string with unix-style slashes for directory names
  // (these slashes are automatically converted to platform-specific
  // path separators).
  //
  // Alternatively, if the argument is an object, it should contain
  // the following attributes:
  //
  //   path: the path to the file, relative to the current user's
  //   profile dir.
  //
  //   autoCreate: whether or not the file should be created if it
  //   doesn't already exist.
  getProfileFile: function getProfileFile(arg) {
    if (typeof arg == "string")
      arg = {path: arg};

    let pathParts = arg.path.split("/");
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.QueryInterface(Ci.nsILocalFile);
    for (let i = 0; i < pathParts.length; i++)
      file.append(pathParts[i]);
    if (arg.autoCreate && !file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);
    return file;
  },

  getLoginManager: function getLoginManager() {
    return Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  },

  makeNewLoginInfo: function getNewLoginInfo() {
    return new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1",
      Ci.nsILoginInfo,
      "init"
    );
  },

  findPassword: function findPassword(realm, username) {
    // fixme: make a request and get the realm ?
    let password;
    let lm = Cc["@mozilla.org/login-manager;1"]
             .getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, 'chrome://weave', null, realm);

    for (let i = 0; i < logins.length; i++) {
      if (logins[i].username == username) {
        password = logins[i].password;
        break;
      }
    }
    return password;
  },

  setPassword: function setPassword(realm, username, password) {
    // cleanup any existing passwords
    let lm = Cc["@mozilla.org/login-manager;1"]
             .getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, 'chrome://weave', null, realm);
    for (let i = 0; i < logins.length; i++)
      lm.removeLogin(logins[i]);

    if (!password)
      return;

    let log = Log4Moz.repository.getLogger("Service.Util");
    log.trace("Setting '" + realm + "' password for user " + username);

    // save the new one
    let nsLoginInfo = new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
    let login = new nsLoginInfo('chrome://weave', null, realm,
                                username, password, "", "");
    lm.addLogin(login);
  },

  // lazy load objects from a constructor on first access.  It will
  // work with the global object ('this' in the global context).
  lazy: function Weave_lazy(dest, prop, ctr) {
    let getter = function() {
      delete dest[prop];
      dest[prop] = new ctr();
      return dest[prop];
    };
    dest.__defineGetter__(prop, getter);
  },

  // like lazy, but rather than new'ing the 3rd arg we use its return value
  lazy2: function Weave_lazy2(dest, prop, fn) {
    let getter = function() {
      delete dest[prop];
      dest[prop] = ctr();
      return dest[prop];
    };
    dest.__defineGetter__(prop, getter);
  },

  lazySvc: function Weave_lazySvc(dest, prop, cid, iface) {
    let getter = function() {
      delete dest[prop];
      dest[prop] = Cc[cid].getService(iface);
      return dest[prop];
    };
    dest.__defineGetter__(prop, getter);
  },

  lazyInstance: function Weave_lazyInstance(dest, prop, cid, iface) {
    let getter = function() {
      delete dest[prop];
      dest[prop] = Cc[cid].createInstance(iface);
      return dest[prop];
    };
    dest.__defineGetter__(prop, getter);
  },

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

    if ("Array" == a.constructor.name && "Array" == b.constructor.name) {
      if (a.length != b.length)
        return false;

      for (let i = 0; i < a.length; i++) {
        if (!Utils.deepEquals(a[i], b[i]))
          return false;
      }

    } else {
      // check if only one of them is an array
      if ("Array" == a.constructor.name || "Array" == b.constructor.name)
        return false;

      for (let key in a) {
        if (!Utils.deepEquals(a[key], b[key]))
          return false;
      }
    }

    return true;
  },

  deepCopy: function Weave_deepCopy(thing, noSort) {
    if (typeof(thing) != "object" || thing == null)
      return thing;
    let ret;

    if ("Array" == thing.constructor.name) {
      ret = [];
      for (let i = 0; i < thing.length; i++)
        ret.push(Utils.deepCopy(thing[i], noSort));

    } else {
      ret = {};
      let props = [p for (p in thing)];
      if (!noSort)
        props = props.sort();
      props.forEach(function(k) ret[k] = Utils.deepCopy(thing[k], noSort));
    }

    return ret;
  },

  // Works on frames or exceptions, munges file:// URIs to shorten the paths
  // FIXME: filename munging is sort of hackish, might be confusing if
  // there are multiple extensions with similar filenames
  formatFrame: function Utils_formatFrame(frame) {
    let tmp = "<file:unknown>";

    if (frame.filename)
      tmp = frame.filename.replace(/^file:\/\/.*\/([^\/]+.js)$/, "module:$1");
    else if (frame.fileName)
      tmp = frame.fileName.replace(/^file:\/\/.*\/([^\/]+.js)$/, "module:$1");

    // skip async.js frames
    if (tmp == "module:async.js")
      return null;

    if (frame.lineNumber)
      tmp += ":" + frame.lineNumber;
    if (frame.name)
      tmp += " :: " + frame.name;

    return tmp;
  },

  exceptionStr: function Weave_exceptionStr(e) {
    let message = e.message ? e.message : e;
    let location = "";

    if (e.location) // Wrapped nsIException.
      location = e.location;
    else if (e.fileName && e.lineNumber) // Standard JS exception
      location = Utils.formatFrame(e);

    if (location)
      location = " (" + location + ")";
    return message + location;
 },

  stackTraceFromFrame: function Weave_stackTraceFromFrame(frame, formatter) {
    if (!formatter)
      formatter = function defaultFormatter(frame) { return frame; };

    let output = "";

    while (frame) {
      let str = formatter(frame);
      if (str)
        output += str + "\n";
      frame = frame.caller;
    }

    return output;
  },

  stackTrace: function Weave_stackTrace(e, formatter) {
    if (e.asyncStack) // AsyncException
      return "Original exception: " + Utils.exceptionStr(e.originalException) + "\n" +
             "Async stack trace:\n" + e.asyncStack;
    else if (e.location) // Wrapped nsIException
      return "Stack trace:\n" + this.stackTraceFromFrame(e.location, formatter);
    else if (e.stack) // Standard JS exception
      return "JS Stack trace:\n" + e.stack;
    else
      return "No traceback available";
  },

  checkStatus: function Weave_checkStatus(code, msg, ranges) {
    if (!ranges)
      ranges = [[200,300]];

    for (let i = 0; i < ranges.length; i++) {
      var rng = ranges[i];
      if (typeof(rng) == "object" && code >= rng[0] && code < rng[1])
        return true;
      else if (typeof(rng) == "number" && code == rng) {
        return true;
      }
    }

    if (msg) {
      let log = Log4Moz.repository.getLogger("Service.Util");
      log.error(msg + " Error code: " + code);
    }

    return false;
  },

  ensureStatus: function Weave_ensureStatus(args) {
    if (!Utils.checkStatus.apply(Utils, arguments))
      throw 'checkStatus failed';
  },

  sha1: function Weave_sha1(string) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
      createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
      .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    let data = converter.convertToByteArray(string, {});
    hasher.update(data, data.length);
    let rawHash = hasher.finish(false);

    // return the two-digit hexadecimal code for a byte
    function toHexString(charCode) {
      return ("0" + charCode.toString(16)).slice(-2);
    }

    let hash = [toHexString(rawHash.charCodeAt(i)) for (i in rawHash)].join("");
    return hash;
  },

  makeURI: function Weave_makeURI(URIString) {
    if (!URIString)
      return null;
    try {
      return Svc.IO.newURI(URIString, null, null);
    } catch (e) {
      let log = Log4Moz.repository.getLogger("Service.Util");
      log.debug("Could not create URI: " + e);
      return null;
    }
  },

  xpath: function Weave_xpath(xmlDoc, xpathString) {
    let root = xmlDoc.ownerDocument == null ?
      xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement;
    let nsResolver = xmlDoc.createNSResolver(root);

    return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                           Ci.nsIDOMXPathResult.ANY_TYPE, null);
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

  // Returns a timer that is scheduled to call the given callback as
  // soon as possible.
  makeTimerForCall: function makeTimerForCall(cb) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(new Utils.EventListener(cb),
                           0, timer.TYPE_ONE_SHOT);
    return timer;
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

  bind2: function Async_bind2(object, method) {
    return function innerBind() { return method.apply(object, arguments); };
  },

  __prefs: null,
  get prefs() {
    if (!this.__prefs) {
      this.__prefs = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefService);
      this.__prefs = this.__prefs.getBranch(PREFS_BRANCH);
      this.__prefs.QueryInterface(Ci.nsIPrefBranch2);
    }
    return this.__prefs;
  },

  /*
   * Event listener object
   * Used to handle XMLHttpRequest and nsITimer callbacks
   */

  EventListener: function Weave_EventListener(handler, eventName) {
    this._handler = handler;
    this._eventName = eventName;
    this._log = Log4Moz.repository.getLogger("Async.EventHandler");
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.async")];
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
    //this._log.trace("Timer fired");
    this._handler(timer);
  }
};

/*
 * Commonly-used services
 */

let Svc = {};
Utils.lazyInstance(Svc, 'Json', "@mozilla.org/dom/json;1", Ci.nsIJSON);
Utils.lazySvc(Svc, 'IO', "@mozilla.org/network/io-service;1", Ci.nsIIOService);
