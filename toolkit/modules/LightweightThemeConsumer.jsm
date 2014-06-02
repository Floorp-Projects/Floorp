/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

this.LightweightThemeConsumer =
 function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._win = aDocument.defaultView;
  this._footerId = aDocument.documentElement.getAttribute("lightweightthemesfooter");

  if (PrivateBrowsingUtils.isWindowPrivate(this._win) &&
      !PrivateBrowsingUtils.permanentPrivateBrowsing) {
    return;
  }

  let screen = this._win.screen;
  this._lastScreenWidth = screen.width;
  this._lastScreenHeight = screen.height;

  Services.obs.addObserver(this, "lightweight-theme-styling-update", false);

  var temp = {};
  Cu.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
  this._update(temp.LightweightThemeManager.currentThemeForDisplay);
  this._win.addEventListener("resize", this);
}

LightweightThemeConsumer.prototype = {
  _lastData: null,
  _lastScreenWidth: null,
  _lastScreenHeight: null,
  // Whether the active lightweight theme should be shown on the window.
  _enabled: true,
  // Whether a lightweight theme is enabled.
  _active: false,

  enable: function() {
    this._enabled = true;
    this._update(this._lastData);
  },

  disable: function() {
    // Dance to keep the data, but reset the applied styles:
    let lastData = this._lastData
    this._update(null);
    this._enabled = false;
    this._lastData = lastData;
  },

  getData: function() {
    return this._enabled ? Cu.cloneInto(this._lastData, this._win) : null;
  },

  observe: function (aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    this._update(JSON.parse(aData));
  },

  handleEvent: function (aEvent) {
    let {width, height} = this._win.screen;

    if (this._lastScreenWidth != width || this._lastScreenHeight != height) {
      this._lastScreenWidth = width;
      this._lastScreenHeight = height;
      if (!this._active)
        return;
      this._update(this._lastData);
      Services.obs.notifyObservers(this._win, "lightweight-theme-optimized",
                                   JSON.stringify(this._lastData));
    }
  },

  destroy: function () {
    if (!PrivateBrowsingUtils.isWindowPrivate(this._win) ||
        PrivateBrowsingUtils.permanentPrivateBrowsing) {
      Services.obs.removeObserver(this, "lightweight-theme-styling-update");

      this._win.removeEventListener("resize", this);
    }

    this._win = this._doc = null;
  },

  _update: function (aData) {
    if (!aData) {
      aData = { headerURL: "", footerURL: "", textcolor: "", accentcolor: "" };
      this._lastData = aData;
    } else {
      this._lastData = aData;
      aData = LightweightThemeImageOptimizer.optimize(aData, this._win.screen);
    }
    if (!this._enabled)
      return;

    let root = this._doc.documentElement;
    let active = !!aData.headerURL;
    let stateChanging = (active != this._active);

    if (active) {
      root.style.color = aData.textcolor || "black";
      root.style.backgroundColor = aData.accentcolor || "white";
      let [r, g, b] = _parseRGB(this._doc.defaultView.getComputedStyle(root, "").color);
      let luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
      root.setAttribute("lwthemetextcolor", luminance <= 110 ? "dark" : "bright");
      root.setAttribute("lwtheme", "true");
    } else {
      root.style.color = "";
      root.style.backgroundColor = "";
      root.removeAttribute("lwthemetextcolor");
      root.removeAttribute("lwtheme");
    }

    this._active = active;

    _setImage(root, active, aData.headerURL);
    if (this._footerId) {
      let footer = this._doc.getElementById(this._footerId);
      footer.style.backgroundColor = active ? aData.accentcolor || "white" : "";
      _setImage(footer, active, aData.footerURL);
      if (active && aData.footerURL)
        footer.setAttribute("lwthemefooter", "true");
      else
        footer.removeAttribute("lwthemefooter");
    }

#ifdef XP_MACOSX
    // On OS X, we extend the lightweight theme into the titlebar, which means setting
    // the chromemargin attribute. Some XUL applications already draw in the titlebar,
    // so we need to save the chromemargin value before we overwrite it with the value
    // that lets us draw in the titlebar. We stash this value on the root attribute so
    // that XUL applications have the ability to invalidate the saved value.
    if (stateChanging) {
      if (!root.hasAttribute("chromemargin-nonlwtheme")) {
        root.setAttribute("chromemargin-nonlwtheme", root.getAttribute("chromemargin"));
      }

      if (active) {
        root.setAttribute("chromemargin", "0,-1,-1,-1");
      } else {
        let defaultChromemargin = root.getAttribute("chromemargin-nonlwtheme");
        if (defaultChromemargin) {
          root.setAttribute("chromemargin", defaultChromemargin);
        } else {
          root.removeAttribute("chromemargin");
        }
      }
    }
#endif
    Services.obs.notifyObservers(this._win, "lightweight-theme-window-updated",
                                 JSON.stringify(aData));
  }
}

function _setImage(aElement, aActive, aURL) {
  aElement.style.backgroundImage =
    (aActive && aURL) ? 'url("' + aURL.replace(/"/g, '\\"') + '")' : "";
}

function _parseRGB(aColorString) {
  var rgb = aColorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
  rgb.shift();
  return rgb.map(function (x) parseInt(x));
}
