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

const EXPORTED_SYMBOLS = ['deepEquals', 'makeFile', 'makeURI', 'xpath',
			  'bind2', 'generatorAsync', 'generatorDone', 'EventListener'];

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

function deepEquals(a, b) {
  if (!a && !b)
    return true;
  if (!a || !b)
    return false;

  if (typeof(a) != "object" && typeof(b) != "object")
    return a == b;
  if (typeof(a) != "object" || typeof(b) != "object")
    return false;

  for (let key in a) {
    if (typeof(a[key]) == "object") {
      if (!typeof(b[key]) == "object")
        return false;
      if (!deepEquals(a[key], b[key]))
        return false;
    } else {
      if (a[key] != b[key])
        return false;
    }
  }
  return true;
}

function makeFile(path) {
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return file;
}

function makeURI(URIString) {
  if (URIString === null || URIString == "")
    return null;
  let ioservice = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  return ioservice.newURI(URIString, null, null);
}

function xpath(xmlDoc, xpathString) {
  let root = xmlDoc.ownerDocument == null ?
    xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement
  let nsResolver = xmlDoc.createNSResolver(root);

  return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                         Ci.nsIDOMXPathResult.ANY_TYPE, null);
}

function bind2(object, method) {
  return function innerBind() { return method.apply(object, arguments); }
}

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

function generatorAsync(self, extra_args) {
  try {
    let args = Array.prototype.slice.call(arguments, 1);
    let gen = this.apply(self, args);
    gen.next(); // must initialize before sending
    gen.send([gen, function(data) {continueGenerator(gen, data);}]);
    return gen;
  } catch (e) {
    if (e instanceof StopIteration) {
      dump("async warning: generator stopped unexpectedly");
      return null;
    } else {
      dump("Exception caught: " + e.message);
    }
  }
}

function continueGenerator(generator, data) {
  try { generator.send(data); }
  catch (e) {
    if (e instanceof StopIteration)
      dump("continueGenerator warning: generator stopped unexpectedly");
    else
      dump("Exception caught: " + e.message);
  }
}

// generators created using Function.async can't simply call the
// callback with the return value, since that would cause the calling
// function to end up running (after the yield) from inside the
// generator.  Instead, generators can call this method which sets up
// a timer to call the callback from a timer (and cleans up the timer
// to avoid leaks).  It also closes generators after the timeout, to
// keep things clean.
function generatorDone(object, generator, callback, retval) {
  if (object._timer)
    throw "Called generatorDone when there is a timer already set."

  let cb = bind2(object, function(event) {
    generator.close();
    generator = null;
    object._timer = null;
    if (callback)
      callback(retval);
  });

  object._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  object._timer.initWithCallback(new EventListener(cb),
                                 0, object._timer.TYPE_ONE_SHOT);
}

/*
 * Event listener object
 * Used to handle XMLHttpRequest and nsITimer callbacks
 */

function EventListener(handler, eventName) {
  this._handler = handler;
  this._eventName = eventName;
  this._log = Log4Moz.Service.getLogger("Service.EventHandler");
}
EventListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsISupports]),

  // DOM event listener
  handleEvent: function EL_handleEvent(event) {
    this._log.debug("Handling event " + this._eventName);
    this._handler(event);
  },

  // nsITimerCallback
  notify: function EL_notify(timer) {
    this._log.trace("Timer fired");
    this._handler(timer);
  }
};
