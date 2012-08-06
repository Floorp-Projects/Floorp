// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// -----------------------------------------------------------
// General util/convenience tools
//

let Util = {
  /** printf-like dump function */
  dumpf: function dumpf(str) {
    let args = arguments;
    let i = 1;
    dump(str.replace(/%s/g, function() {
      if (i >= args.length) {
        throw "dumps received too many placeholders and not enough arguments";
      }
      return args[i++].toString();
    }));
  },

  /** Like dump, but each arg is handled and there's an automatic newline */
  dumpLn: function dumpLn() {
    for (let i = 0; i < arguments.length; i++)
      dump(arguments[i] + " ");
    dump("\n");
  },

  getWindowUtils: function getWindowUtils(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
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
    return (aURL.indexOf("about:") == 0 && aURL != "about:blank" && aURL != "about:empty") || aURL.indexOf("chrome:") == 0;
  },

  isOpenableScheme: function isShareableScheme(aProtocol) {
    let dontOpen = /^(mailto|javascript|news|snews)$/;
    return (aProtocol && !dontOpen.test(aProtocol));
  },

  isShareableScheme: function isShareableScheme(aProtocol) {
    let dontShare = /^(chrome|about|file|javascript|resource)$/;
    return (aProtocol && !dontShare.test(aProtocol));
  },

  clamp: function(num, min, max) {
    return Math.max(min, Math.min(max, num));
  },

  /** Don't display anything in the urlbar for these special URIs. */
  isURLEmpty: function isURLEmpty(aURL) {
    return (!aURL || aURL == "about:blank" || aURL == "about:empty" || aURL == "about:home");
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
    Services.io.offline = false;
  },

  isParentProcess: function isInParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
  },

  isTablet: function isTablet(options) {
    let forceUpdate = options && 'forceUpdate' in options && options.forceUpdate;

    if ('_isTablet' in this && !forceUpdate)
      return this._isTablet;

    let tabletPref = Services.prefs.getIntPref("browser.ui.layout.tablet");

    // Act according to user prefs if tablet mode has been
    // explicitly disabled or enabled.
    if (tabletPref == 0)
      return this._isTablet = false;
    else if (tabletPref == 1)
      return this._isTablet = true;

#ifdef ANDROID
    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    let shellVersion = sysInfo.get("shellVersion") || "";
    let matches = shellVersion.match(/\((\d+)\)$/);
    if (matches) {
      let sdkVersion = parseInt(matches[1]);
      // Disable tablet mode on pre-honeycomb devices because of theme bugs (bug 705026)
      if (sdkVersion < 11)
        return this._isTablet = false;

      // Always enable tablet mode on honeycomb devices.
      if (sdkVersion < 14)
        return this._isTablet = true;
    }
    // On Ice Cream Sandwich devices, switch modes based on screen size.
    return this._isTablet = sysInfo.get("tablet");
#endif

    let dpi = this.displayDPI;
    if (dpi <= 96)
      return this._isTablet = (window.innerWidth > 1024);

    // See the tablet_panel_minwidth from mobile/themes/core/defines.inc
    let tablet_panel_minwidth = 124;
    let dpmm = 25.4 * window.innerWidth / dpi;
    return this._isTablet = (dpmm >= tablet_panel_minwidth);
  },

  isPortrait: function isPortrait() {
#ifdef MOZ_PLATFORM_MAEMO
    return (screen.width <= screen.height);
#elifdef ANDROID
    return (screen.width <= screen.height);
#else
    return (window.innerWidth <= window.innerHeight);
#endif
  },

  modifierMaskFromEvent: function modifierMaskFromEvent(aEvent) {
    return (aEvent.altKey   ? Ci.nsIDOMEvent.ALT_MASK     : 0) |
           (aEvent.ctrlKey  ? Ci.nsIDOMEvent.CONTROL_MASK : 0) |
           (aEvent.shiftKey ? Ci.nsIDOMEvent.SHIFT_MASK   : 0) |
           (aEvent.metaKey  ? Ci.nsIDOMEvent.META_MASK    : 0);
  },

  get displayDPI() {
    delete this.displayDPI;
    return this.displayDPI = this.getWindowUtils(window).displayDPI;
  },

  LOCALE_DIR_RTL: -1,
  LOCALE_DIR_LTR: 1,
  get localeDir() {
    // determine browser dir first to know which direction to snap to
    let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
    return chromeReg.isLocaleRTL("global") ? this.LOCALE_DIR_RTL : this.LOCALE_DIR_LTR;
  },

  createShortcut: function Util_createShortcut(aTitle, aURL, aIconURL, aType) {
    // The background images are 72px, but Android will resize as needed.
    // Bigger is better than too small.
    const kIconSize = 72;
    const kOverlaySize = 32;
    const kOffset = 20;

    // We have to fallback to something
    aTitle = aTitle || aURL;

    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.setAttribute("style", "display: none");

    function _createShortcut() {
      let icon = canvas.toDataURL("image/png", "");
      canvas = null;
      try {
        let shell = Cc["@mozilla.org/browser/shell-service;1"].createInstance(Ci.nsIShellService);
        shell.createShortcut(aTitle, aURL, icon, aType);
      } catch(e) {
        Cu.reportError(e);
      }
    }

    // Load the main background image first
    let image = new Image();
    image.onload = function() {
      canvas.width = canvas.height = kIconSize;
      let ctx = canvas.getContext("2d");
      ctx.drawImage(image, 0, 0, kIconSize, kIconSize);

      // If we have a favicon, lets draw it next
      if (aIconURL) {
        let favicon = new Image();
        favicon.onload = function() {
          // Center the favicon and overlay it on the background
          ctx.drawImage(favicon, kOffset, kOffset, kOverlaySize, kOverlaySize);
          _createShortcut();
        }

        favicon.onerror = function() {
          Cu.reportError("CreateShortcut: favicon image load error");
        }

        favicon.src = aIconURL;
      } else {
        _createShortcut();
      }
    }

    image.onerror = function() {
      Cu.reportError("CreateShortcut: background image load error");
    }

    // Pick the right background
    image.src = aIconURL ? "chrome://browser/skin/images/homescreen-blank-hdpi.png"
                         : "chrome://browser/skin/images/homescreen-default-hdpi.png";
  },
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
    if (this.isPending()) {
      this._timer.cancel();
      this._type = null;
    }
    return this;
  },

  /** If there is a pending timeout, call it and cancel the timeout. */
  flush: function flush() {
    if (this.isPending()) {
      this.notify();
      this.clear();
    }
    return this;
  },

  /** Return true iff we are waiting for a callback. */
  isPending: function isPending() {
    return this._type !== null;
  }
};

