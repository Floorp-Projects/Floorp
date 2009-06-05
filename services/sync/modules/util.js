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
Cu.import("resource://weave/ext/Preferences.js");
Cu.import("resource://weave/ext/Observers.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/log4moz.js");

/*
 * Utility functions
 */

let Utils = {
  /**
   * Wrap functions to notify when it starts and finishes executing or if it got
   * an error. The message is a combination of a provided prefix and local name
   * with the current state and the subject is the provided subject.
   *
   * @usage function MyObj() { this._notify = Utils.notify("prefix:"); }
   *        MyObj.foo = function() { this._notify(name, subject, func)(); }
   */
  notify: function Utils_notify(prefix) {
    return function NotifyMaker(name, subject, func) {
      let thisArg = this;
      let notify = function(state) {
        let mesg = prefix + name + ":" + state;
        thisArg._log.debug("Event: " + mesg);
        Observers.notify(mesg, subject);
      };

      return function WrappedNotify() {
        try {
          notify("start");
          let ret = func.call(thisArg);
          notify("finish");
          return ret;
        }
        catch(ex) {
          notify("error");
          throw ex;
        }
      };
    };
  },

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
    let file = Svc.Directory.get("ProfD", Ci.nsIFile);
    file.QueryInterface(Ci.nsILocalFile);
    for (let i = 0; i < pathParts.length; i++)
      file.append(pathParts[i]);
    if (arg.autoCreate && !file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);
    return file;
  },

  findPassword: function findPassword(realm, username) {
    // fixme: make a request and get the realm ?
    let password;
    let logins = Svc.Login.findLogins({}, 'chrome://weave', null, realm);

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
    let lm = Svc.Login;
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

  /**
   * Add a simple getter/setter to an object that defers access of a property
   * to an inner property.
   *
   * @param obj
   *        Object to add properties to defer in its prototype
   * @param defer
   *        Hash property of obj to defer to (dot split each level)
   * @param prop
   *        Property name to defer (or an array of property names)
   */
  deferGetSet: function Utils_deferGetSet(obj, defer, prop) {
    if (Utils.isArray(prop))
      return prop.map(function(prop) Utils.deferGetSet(obj, defer, prop));

    // Split the defer into each dot part for each level to dereference
    let parts = defer.split(".");
    let deref = function(base) Utils.deref(base, parts);

    let prot = obj.prototype;

    // Create a getter if it doesn't exist yet
    if (!prot.__lookupGetter__(prop))
      prot.__defineGetter__(prop, function() deref(this)[prop]);

    // Create a setter if it doesn't exist yet
    if (!prot.__lookupSetter__(prop))
      prot.__defineSetter__(prop, function(val) deref(this)[prop] = val);
  },

  /**
   * Dereference an array of properties starting from a base object
   *
   * @param base
   *        Base object to start dereferencing
   * @param props
   *        Array of properties to dereference (one for each level)
   */
  deref: function Utils_deref(base, props) props.reduce(function(curr, prop)
    curr[prop], base),

  /**
   * Determine if some value is an array
   *
   * @param val
   *        Value to check (can be null, undefined, etc.)
   * @return True if it's an array; false otherwise
   */
  isArray: function Utils_isArray(val) val != null && typeof val == "object" &&
    val.constructor.name == "Array",

  // lazy load objects from a constructor on first access.  It will
  // work with the global object ('this' in the global context).
  lazy: function Weave_lazy(dest, prop, ctr) {
    delete dest[prop];
    dest.__defineGetter__(prop, Utils.lazyCb(dest, prop, ctr));
  },
  lazyCb: function Weave_lazyCb(dest, prop, ctr) {
    return function() {
      delete dest[prop];
      dest[prop] = new ctr();
      return dest[prop];
    };
  },

  // like lazy, but rather than new'ing the 3rd arg we use its return value
  lazy2: function Weave_lazy2(dest, prop, fn) {
    delete dest[prop];
    dest.__defineGetter__(prop, Utils.lazyCb2(dest, prop, fn));
  },
  lazyCb2: function Weave_lazyCb2(dest, prop, fn) {
    return function() {
      delete dest[prop];
      return dest[prop] = fn();
    };
  },

  lazySvc: function Weave_lazySvc(dest, prop, cid, iface) {
    let getter = function() {
      delete dest[prop];
      if (!Cc[cid]) {
        let log = Log4Moz.repository.getLogger("Service.Util");
        log.warn("Component " + cid + " requested, but doesn't exist on "
                 + "this platform.");
	return null;
      } else{
        dest[prop] = Cc[cid].getService(iface);
        return dest[prop];
      }
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

  deepEquals: function eq(a, b) {
    // If they're triple equals, then it must be equals!
    if (a === b)
      return true;

    // Grab the keys from both sides
    let [A, B] = [[i for (i in a)], [i for (i in b)]];
    // Don't bother doing any more if they aren't objects with same # keys
    if (typeof a != "object" || typeof b != "object" || A.length != B.length)
      return false;

    // Check if both sides have the same keys and same value for the key
    return A.every(function(A) B.some(function(B) A == B && eq(a[A], b[B])));
  },

  deepCopy: function Weave_deepCopy(thing, noSort) {
    if (typeof(thing) != "object" || thing == null)
      return thing;
    let ret;

    if (Utils.isArray(thing)) {
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

  makeURL: function Weave_makeURL(URIString) {
    let url = Utils.makeURI(URIString);
    url.QueryInterface(Ci.nsIURL);
    return url;
  },

  xpath: function Weave_xpath(xmlDoc, xpathString) {
    let root = xmlDoc.ownerDocument == null ?
      xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement;
    let nsResolver = xmlDoc.createNSResolver(root);

    return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                           Ci.nsIDOMXPathResult.ANY_TYPE, null);
  },

  getTmp: function Weave_getTmp(name) {
    let tmp = Svc.Directory.get("ProfD", Ci.nsIFile);
    tmp.QueryInterface(Ci.nsILocalFile);

    tmp.append("weave");
    tmp.append("tmp");
    if (!tmp.exists())
      tmp.create(tmp.DIRECTORY_TYPE, PERMS_DIRECTORY);

    if (name)
      tmp.append(name);

    return tmp;
  },

  /**
   * Load a json object from disk
   *
   * @param filePath
   *        Json file path load from weave/[filePath].json
   * @param that
   *        Object to use for logging and "this" for callback
   * @param callback
   *        Function to process json object as its first parameter
   */
  jsonLoad: function Utils_jsonLoad(filePath, that, callback) {
    filePath = "weave/" + filePath + ".json";
    if (that._log)
      that._log.debug("Loading json from disk: " + filePath);

    let file = Utils.getProfileFile(filePath);
    if (!file.exists())
      return;

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      callback.call(that, JSON.parse(json));
    }
    catch (ex) {
      if (that._log)
        that._log.debug("Failed to load json: " + ex);
    }
  },

  /**
   * Save a json-able object to disk
   *
   * @param filePath
   *        Json file path save to weave/[filePath].json
   * @param that
   *        Object to use for logging and "this" for callback
   * @param callback
   *        Function to provide json-able object to save. If this isn't a
   *        function, it'll be used as the object to make a json string.
   */
  jsonSave: function Utils_jsonSave(filePath, that, callback) {
    filePath = "weave/" + filePath + ".json";
    if (that._log)
      that._log.debug("Saving json to disk: " + filePath);

    let file = Utils.getProfileFile({ autoCreate: true, path: filePath });
    let json = typeof callback == "function" ? callback.call(that) : callback;
    let out = JSON.stringify(json);
    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
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

  /**
   * Open/reshow a window/dialog based on its name and type.
   *
   * @param name
   *        Name of the window/dialog to reshow if already open
   * @param type
   *        Opening behavior: "Window" or "Dialog"
   * @param args
   *        More arguments go here depending on the type
   */
  _openWin: function Utils__openWin(name, type /*, args... */) {
    // Just re-show the window if it's already open
    let openedWindow = Svc.WinMediator.getMostRecentWindow("Weave:" + name);
    if (openedWindow) {
      openedWindow.focus();
      return;
    }

    // Open up the window/dialog!
    let win = Svc.WinWatcher;
    if (type == "Dialog")
      win = win.activeWindow;
    win["open" + type].apply(win, Array.slice(arguments, 2));
  },

  openDialog: function Utils_openDialog(name, uri, options, args) {
    Utils._openWin(name, "Dialog", "chrome://weave/content/" + uri, "",
      options || "centerscreen,chrome,dialog,modal,resizable=no", args);
  },

  openLog: function Utils_openLog() {
    Utils.openWindow("Log", "log.xul");
  },

  openLogin: function Utils_openLogin() {
    Utils.openDialog("Login", "login.xul");
  },

  openShare: function Utils_openShare() {
    Utils.openDialog("Share", "share.xul");
  },

  openStatus: function Utils_openStatus() {
    Utils.openWindow("Status", "status.xul");
  },

  openSync: function Utils_openSync() {
    Utils.openWindow("Sync", "pick-sync.xul");
  },

  openWindow: function Utils_openWindow(name, uri, options, args) {
    Utils._openWin(name, "Window", null, "chrome://weave/content/" + uri,
      "", options || "centerscreen,chrome,dialog,resizable=yes", args);
  },

  openWizard: function Utils_openWizard() {
    Utils.openWindow("Wizard", "wizard.xul");
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
Svc.Prefs = new Preferences(PREFS_BRANCH);
[["AppInfo", "@mozilla.org/xre/app-info;1", "nsIXULAppInfo"],
 ["Crypto", "@labs.mozilla.com/Weave/Crypto;1", "IWeaveCrypto"],
 ["Directory", "@mozilla.org/file/directory_service;1", "nsIProperties"],
 ["IO", "@mozilla.org/network/io-service;1", "nsIIOService"],
 ["Login", "@mozilla.org/login-manager;1", "nsILoginManager"],
 ["Memory", "@mozilla.org/xpcom/memory-service;1", "nsIMemory"],
 ["Observer", "@mozilla.org/observer-service;1", "nsIObserverService"],
 ["Private", "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService"],
 ["Prompt", "@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService"],
 ["Version", "@mozilla.org/xpcom/version-comparator;1", "nsIVersionComparator"],
 ["WinMediator", "@mozilla.org/appshell/window-mediator;1", "nsIWindowMediator"],
 ["WinWatcher", "@mozilla.org/embedcomp/window-watcher;1", "nsIWindowWatcher"],
].forEach(function(lazy) Utils.lazySvc(Svc, lazy[0], lazy[1], Ci[lazy[2]]));
