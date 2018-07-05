/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const toolkitVariableMap = [
  ["--lwt-accent-color", {
    lwtProperty: "accentcolor",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels || rgbaChannels.a == 0) {
        return "white";
      }
      // Remove the alpha channel
      const {r, g, b} = rgbaChannels;
      return `rgb(${r}, ${g}, ${b})`;
    }
  }],
  ["--lwt-text-color", {
    lwtProperty: "textcolor",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels) {
        rgbaChannels = {r: 0, g: 0, b: 0};
      }
      // Remove the alpha channel
      const {r, g, b} = rgbaChannels;
      element.setAttribute("lwthemetextcolor", _isTextColorDark(r, g, b) ? "dark" : "bright");
      return `rgba(${r}, ${g}, ${b})`;
    }
  }],
  ["--arrowpanel-background", {
    lwtProperty: "popup"
  }],
  ["--arrowpanel-color", {
    lwtProperty: "popup_text",
    processColor(rgbaChannels, element) {
      const disabledColorVariable = "--panel-disabled-color";

      if (!rgbaChannels) {
        element.removeAttribute("lwt-popup-brighttext");
        element.removeAttribute("lwt-popup-darktext");
        element.style.removeProperty(disabledColorVariable);
        return null;
      }

      let {r, g, b, a} = rgbaChannels;

      if (_isTextColorDark(r, g, b)) {
        element.removeAttribute("lwt-popup-brighttext");
        element.setAttribute("lwt-popup-darktext", "true");
      } else {
        element.removeAttribute("lwt-popup-darktext");
        element.setAttribute("lwt-popup-brighttext", "true");
      }

      element.style.setProperty(disabledColorVariable, `rgba(${r}, ${g}, ${b}, 0.5)`);
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    }
  }],
  ["--arrowpanel-border-color", {
    lwtProperty: "popup_border"
  }],
  ["--lwt-toolbar-field-background-color", {
    lwtProperty: "toolbar_field"
  }],
  ["--lwt-toolbar-field-color", {
    lwtProperty: "toolbar_field_text",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels) {
        element.removeAttribute("lwt-toolbar-field-brighttext");
        return null;
      }
      const {r, g, b, a} = rgbaChannels;
      if (_isTextColorDark(r, g, b)) {
        element.removeAttribute("lwt-toolbar-field-brighttext");
      } else {
        element.setAttribute("lwt-toolbar-field-brighttext", "true");
      }
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    }
  }],
  ["--lwt-toolbar-field-border-color", {
    lwtProperty: "toolbar_field_border"
  }],
  ["--lwt-toolbar-field-focus", {
    lwtProperty: "toolbar_field_focus"
  }],
  ["--lwt-toolbar-field-focus-color", {
    lwtProperty: "toolbar_field_text_focus"
  }],
  ["--toolbar-field-focus-border-color", {
    lwtProperty: "toolbar_field_border_focus"
  }],
];

// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineModuleGetter(this, "ThemeContentPropertyList",
  "resource:///modules/ThemeVariableMap.jsm");
ChromeUtils.defineModuleGetter(this, "ThemeVariableMap",
  "resource:///modules/ThemeVariableMap.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._win = aDocument.defaultView;

  Services.obs.addObserver(this, "lightweight-theme-styling-update");

  var temp = {};
  ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
  this._update(temp.LightweightThemeManager.currentThemeForDisplay);

  this._win.addEventListener("resolutionchange", this);
  this._win.addEventListener("unload", this, { once: true });
  this._win.addEventListener("EndSwapDocShells", this, true);
  this._win.messageManager.addMessageListener("LightweightTheme:Request", this);

  let darkThemeMediaQuery = this._win.matchMedia("(-moz-system-dark-theme)");
  darkThemeMediaQuery.addListener(temp.LightweightThemeManager);
  temp.LightweightThemeManager.systemThemeChanged(darkThemeMediaQuery);
}

LightweightThemeConsumer.prototype = {
  _lastData: null,
  // Whether a lightweight theme is enabled.
  _active: false,

  observe(aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    const { outerWindowID } = this._win
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);

    const parsedData = JSON.parse(aData);
    if (parsedData && parsedData.window && parsedData.window !== outerWindowID) {
      return;
    }

    this._update(parsedData);
  },

  receiveMessage({ name, target }) {
    if (name == "LightweightTheme:Request") {
      let contentThemeData = _getContentProperties(this._doc, this._active, this._lastData);
      target.messageManager.sendAsyncMessage("LightweightTheme:Update", contentThemeData);
    }
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resolutionchange":
        if (this._active) {
          this._update(this._lastData);
        }
        break;
      case "unload":
        Services.obs.removeObserver(this, "lightweight-theme-styling-update");
        this._win.removeEventListener("resolutionchange", this);
        this._win.removeEventListener("EndSwapDocShells", this, true);
        this._win = this._doc = null;
        break;
      case "EndSwapDocShells":
        let contentThemeData = _getContentProperties(this._doc, this._active, this._lastData);
        aEvent.target.messageManager.sendAsyncMessage("LightweightTheme:Update", contentThemeData);
        break;
    }
  },

  _update(aData) {
    if (!aData) {
      aData = { headerURL: "", footerURL: "", textcolor: "", accentcolor: "" };
      this._lastData = aData;
    } else {
      this._lastData = aData;
      aData = LightweightThemeImageOptimizer.optimize(aData, this._win.screen);
    }

    let root = this._doc.documentElement;

    if (aData.headerURL) {
      root.setAttribute("lwtheme-image", "true");
    } else {
      root.removeAttribute("lwtheme-image");
    }

    let active = aData.accentcolor || aData.headerURL;
    this._active = active;

    if (aData.icons) {
      let activeIcons = active ? Object.keys(aData.icons).join(" ") : "";
      root.setAttribute("lwthemeicons", activeIcons);
      for (let [name, value] of Object.entries(aData.icons)) {
        _setImage(root, active, name, value);
      }
    } else {
      root.removeAttribute("lwthemeicons");
    }

    _setImage(root, active, "--lwt-header-image", aData.headerURL);
    _setImage(root, active, "--lwt-footer-image", aData.footerURL);
    _setImage(root, active, "--lwt-additional-images", aData.additionalBackgrounds);
    _setProperties(root, active, aData);

    if (active) {
      root.setAttribute("lwtheme", "true");
    } else {
      root.removeAttribute("lwtheme");
      root.removeAttribute("lwthemetextcolor");
    }

    if (active && aData.footerURL)
      root.setAttribute("lwthemefooter", "true");
    else
      root.removeAttribute("lwthemefooter");

    let contentThemeData = _getContentProperties(this._doc, active, aData);

    let browserMessageManager = this._win.getGroupMessageManager("browsers");
    browserMessageManager.broadcastAsyncMessage(
      "LightweightTheme:Update", contentThemeData
    );
  }
};

function _getContentProperties(doc, active, data) {
  if (!active) {
    return {};
  }
  let properties = {};
  for (let property in data) {
    if (ThemeContentPropertyList.includes(property)) {
      properties[property] = _parseRGBA(_sanitizeCSSColor(doc, data[property]));
    }
  }
  return properties;
}

function _setImage(aRoot, aActive, aVariableName, aURLs) {
  if (aURLs && !Array.isArray(aURLs)) {
    aURLs = [aURLs];
  }
  _setProperty(aRoot, aActive, aVariableName, aURLs && aURLs.map(v => `url("${v.replace(/"/g, '\\"')}")`).join(","));
}

function _setProperty(elem, active, variableName, value) {
  if (active && value) {
    elem.style.setProperty(variableName, value);
  } else {
    elem.style.removeProperty(variableName);
  }
}

function _setProperties(root, active, themeData) {
  for (let map of [toolkitVariableMap, ThemeVariableMap]) {
    for (let [cssVarName, definition] of map) {
      const {
        lwtProperty,
        optionalElementID,
        processColor,
        isColor = true
      } = definition;
      let elem = optionalElementID ? root.ownerDocument.getElementById(optionalElementID)
                                   : root;

      let val = themeData[lwtProperty];
      if (isColor) {
        val = _sanitizeCSSColor(root.ownerDocument, val);
        if (processColor) {
          val = processColor(_parseRGBA(val), elem);
        }
      }
      _setProperty(elem, active, cssVarName, val);
    }
  }
}

function _sanitizeCSSColor(doc, cssColor) {
  if (!cssColor) {
    return null;
  }
  const HTML_NS = "http://www.w3.org/1999/xhtml";
  // style.color normalizes color values and makes invalid ones black, so a
  // simple round trip gets us a sanitized color value.
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.color = "black";
  let span = doc.createElementNS(HTML_NS, "span");
  span.style.color = cssColor;
  div.appendChild(span);
  cssColor = doc.defaultView.getComputedStyle(span).color;
  return cssColor;
}

function _parseRGBA(aColorString) {
  if (!aColorString) {
    return null;
  }
  var rgba = aColorString.replace(/(rgba?\()|(\)$)/g, "").split(",");
  rgba = rgba.map(x => parseFloat(x));
  return {
    r: rgba[0],
    g: rgba[1],
    b: rgba[2],
    a: 3 in rgba ? rgba[3] : 1,
  };
}

function _isTextColorDark(r, g, b) {
  return (0.2125 * r + 0.7154 * g + 0.0721 * b) <= 110;
}
