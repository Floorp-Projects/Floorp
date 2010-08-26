// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <rfrostig@mozilla.com>
 *   Ben Combee <bcombee@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
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

let Ci = Components.interfaces;

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

// -----------------------------------------------------------
// General util/convenience tools
//

Cu.import("resource://gre/modules/Geometry.jsm");

let Util = {
  bind: function bind(f, thisObj) {
    return function() {
      return f.apply(thisObj, arguments);
    };
  },

  bindAll: function bindAll(instance) {
    let bind = Util.bind;
    for (let key in instance)
      if (instance[key] instanceof Function)
        instance[key] = bind(instance[key], instance);
  },

  /** printf-like dump function */
  dumpf: function dumpf(str) {
    var args = arguments;
    var i = 1;
    dump(str.replace(/%s/g, function() {
      if (i >= args.length) {
        throw "dumps received too many placeholders and not enough arguments";
      }
      return args[i++].toString();
    }));
  },

  /** Like dump, but each arg is handled and there's an automatic newline */
  dumpLn: function dumpLn() {
    for (var i = 0; i < arguments.length; i++) { dump(arguments[i] + " "); }
    dump("\n");
  },

  getWindowUtils: function getWindowUtils(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
  },

  getScrollOffset: function getScrollOffset(aWindow) {
    var cwu = Util.getWindowUtils(aWindow);
    var scrollX = {};
    var scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
    return new Point(scrollX.value, scrollY.value);
  },

  /** Executes aFunc after other events have been processed. */
  executeSoon: function executeSoon(aFunc) {
    Services.tm.mainThread.dispatch({
      run: function() {
        aFunc();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  },

  getHrefForElement: function getHrefForElement(target) {
    // XXX: This is kind of a hack to work around a Gecko bug (see bug 266932)
    // We're going to walk up the DOM looking for a parent link node.
    // This shouldn't be necessary, but we're matching the existing behaviour for left click

    let link = null;
    while (target) {
      if (target instanceof Ci.nsIDOMHTMLAnchorElement || 
          target instanceof Ci.nsIDOMHTMLAreaElement ||
          target instanceof Ci.nsIDOMHTMLLinkElement) {
          if (target.hasAttribute("href"))
            link = target;
      }
      target = target.parentNode;
    }

    if (link && link.hasAttribute("href"))
      return link.href;
    else
      return null;
  },

  makeURI: function makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  makeURLAbsolute: function makeURLAbsolute(base, url) {
    // Note:  makeURI() will throw if url is not a valid URI
    return this.makeURI(url, null, this.makeURI(base)).spec;
  },

  isLocalScheme: function isLocalScheme(aURL) {
    return (aURL.indexOf("about:") == 0 && aURL != "about:blank") || aURL.indexOf("chrome:") == 0;
  },

  isShareableScheme: function isShareableScheme(aProtocol) {
    let dontShare = /^(chrome|about|file|javascript|resource)$/;
    return (aProtocol && !dontShare.test(aProtocol));
  },

  clamp: function(num, min, max) {
    return Math.max(min, Math.min(max, num));
  },

  /**
   * Determines whether a home page override is needed.
   * Returns:
   *  "new profile" if this is the first run with a new profile.
   *  "new version" if this is the first run with a build with a different
   *                      Gecko milestone (i.e. right after an upgrade).
   *  "none" otherwise.
   */
  needHomepageOverride: function needHomepageOverride() {
    let savedmstone = null;
    try {
      savedmstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
    } catch (e) {}

    if (savedmstone == "ignore")
      return "none";

#expand    let ourmstone = "__MOZ_APP_VERSION__";

    if (ourmstone != savedmstone) {
      Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourmstone);

      return (savedmstone ? "new version" : "new profile");
    }

    return "none";
  },

  /** Don't display anything in the urlbar for these special URIs. */
  isURLEmpty: function isURLEmpty(aURL) {
    return (!aURL || aURL == "about:blank" || aURL == "about:home");
  },

  /** Recursively find all documents, including root document. */
  getAllDocuments: function getAllDocuments(doc, resultSoFar) {
    resultSoFar = resultSoFar || [doc];
    if (!doc.defaultView)
      return resultSoFar;
    let frames = doc.defaultView.frames;
    if (!frames)
      return resultSoFar;

    let i;
    let currentDoc;
    for (i = 0; i < frames.length; i++) {
      currentDoc = frames[i].document;
      resultSoFar.push(currentDoc);
      this.getAllDocuments(currentDoc, resultSoFar);
    }

    return resultSoFar;
  },

  // Put the Mozilla networking code into a state that will kick the auto-connection
  // process.
  forceOnline: function forceOnline() {
#ifdef MOZ_ENABLE_LIBCONIC
    Services.io.offline = false;
#endif
  },

  /** Capitalize first letter of a string. */
  capitalize: function(str) {
    return str.charAt(0).toUpperCase() + str.substring(1);
  },
  
  isPortrait: function isPortrait() {
    return (window.innerWidth < 500);
  }
};


/**
 * Helper class to nsITimer that adds a little more pizazz.  Callback can be an
 * object with a notify method or a function.
 */
Util.Timeout = function(aCallback) {
  this._callback = aCallback;
  this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._type = null;
};

Util.Timeout.prototype = {
  /** Timer callback. Don't call this manually. */
  notify: function notify() {
    if (this._type == this._timer.TYPE_ONE_SHOT)
      this._type = null;

    if (this._callback.notify)
      this._callback.notify();
    else
      this._callback.apply(null);
  },

  /** Helper function for once and interval. */
  _start: function _start(aDelay, aType, aCallback) {
    if (aCallback)
      this._callback = aCallback;
    this.clear();
    this._timer.initWithCallback(this, aDelay, aType);
    this._type = aType;
    return this;
  },

  /** Do the callback once.  Cancels other timeouts on this object. */
  once: function once(aDelay, aCallback) {
    return this._start(aDelay, this._timer.TYPE_ONE_SHOT, aCallback);
  },

  /** Do the callback every aDelay msecs. Cancels other timeouts on this object. */
  interval: function interval(aDelay, aCallback) {
    return this._start(aDelay, this._timer.TYPE_REPEATING_SLACK, aCallback);
  },

  /** Clear any pending timeouts. */
  clear: function clear() {
    if (this._type !== null) {
      this._timer.cancel();
      this._type = null;
    }
    return this;
  },

  /** If there is a pending timeout, call it and cancel the timeout. */
  flush: function flush() {
    if (this._type) {
      this.notify();
      this.clear();
    }
    return this;
  },

  /** Return true iff we are waiting for a callback. */
  isPending: function isPending() {
    return !!this._type;
  }
};



/**
 * Cache of commonly used elements.
 */
let Elements = {};

[
  ["browserBundle",      "bundle_browser"],
  ["contentShowing",     "bcast_contentShowing"],
  ["urlbarState",        "bcast_urlbarState"],
  ["stack",              "stack"],
  ["panelUI",            "panel-container"],
  ["viewBuffer",         "view-buffer"],
  ["toolbarContainer",   "toolbar-container"],
].forEach(function (elementGlobal) {
  let [name, id] = elementGlobal;
  Elements.__defineGetter__(name, function () {
    let element = document.getElementById(id);
    if (!element)
      return null;
    delete Elements[name];
    return Elements[name] = element;
  });
});
