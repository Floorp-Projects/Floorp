/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineModuleGetter(
  this,
  "ThemeContentPropertyList",
  "resource:///modules/ThemeVariableMap.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ThemeVariableMap",
  "resource:///modules/ThemeVariableMap.jsm"
);

const toolkitVariableMap = [
  [
    "--lwt-accent-color",
    {
      lwtProperty: "accentcolor",
      processColor(rgbaChannels, element) {
        if (!rgbaChannels || rgbaChannels.a == 0) {
          return "white";
        }
        // Remove the alpha channel
        const { r, g, b } = rgbaChannels;
        return `rgb(${r}, ${g}, ${b})`;
      },
    },
  ],
  [
    "--lwt-text-color",
    {
      lwtProperty: "textcolor",
      processColor(rgbaChannels, element) {
        if (!rgbaChannels) {
          rgbaChannels = { r: 0, g: 0, b: 0 };
        }
        // Remove the alpha channel
        const { r, g, b } = rgbaChannels;
        element.setAttribute(
          "lwthemetextcolor",
          _isColorDark(r, g, b) ? "dark" : "bright"
        );
        return `rgba(${r}, ${g}, ${b})`;
      },
    },
  ],
  [
    "--arrowpanel-background",
    {
      lwtProperty: "popup",
    },
  ],
  [
    "--arrowpanel-color",
    {
      lwtProperty: "popup_text",
      processColor(rgbaChannels, element) {
        const disabledColorVariable = "--panel-disabled-color";

        if (!rgbaChannels) {
          element.removeAttribute("lwt-popup-brighttext");
          element.style.removeProperty(disabledColorVariable);
          return null;
        }

        let { r, g, b, a } = rgbaChannels;

        if (_isColorDark(r, g, b)) {
          element.removeAttribute("lwt-popup-brighttext");
        } else {
          element.setAttribute("lwt-popup-brighttext", "true");
        }

        element.style.setProperty(
          disabledColorVariable,
          `rgba(${r}, ${g}, ${b}, 0.5)`
        );
        return `rgba(${r}, ${g}, ${b}, ${a})`;
      },
    },
  ],
  [
    "--arrowpanel-border-color",
    {
      lwtProperty: "popup_border",
    },
  ],
  [
    "--lwt-toolbar-field-background-color",
    {
      lwtProperty: "toolbar_field",
    },
  ],
  [
    "--lwt-toolbar-field-color",
    {
      lwtProperty: "toolbar_field_text",
      processColor(rgbaChannels, element) {
        if (!rgbaChannels) {
          element.removeAttribute("lwt-toolbar-field-brighttext");
          return null;
        }
        const { r, g, b, a } = rgbaChannels;
        if (_isColorDark(r, g, b)) {
          element.removeAttribute("lwt-toolbar-field-brighttext");
        } else {
          element.setAttribute("lwt-toolbar-field-brighttext", "true");
        }
        return `rgba(${r}, ${g}, ${b}, ${a})`;
      },
    },
  ],
  [
    "--lwt-toolbar-field-border-color",
    {
      lwtProperty: "toolbar_field_border",
    },
  ],
  [
    "--lwt-toolbar-field-focus",
    {
      lwtProperty: "toolbar_field_focus",
      fallbackProperty: "toolbar_field",
      processColor(rgbaChannels, element, propertyOverrides) {
        // Ensure minimum opacity as this is used behind address bar results.
        if (!rgbaChannels) {
          propertyOverrides.set("toolbar_field_text_focus", "black");
          return "white";
        }
        const min_opacity = 0.9;
        let { r, g, b, a } = rgbaChannels;
        if (a < min_opacity) {
          propertyOverrides.set(
            "toolbar_field_text_focus",
            _isColorDark(r, g, b) ? "white" : "black"
          );
          return `rgba(${r}, ${g}, ${b}, ${min_opacity})`;
        }
        return `rgba(${r}, ${g}, ${b}, ${a})`;
      },
    },
  ],
  [
    "--lwt-toolbar-field-focus-color",
    {
      lwtProperty: "toolbar_field_text_focus",
      fallbackProperty: "toolbar_field_text",
      processColor(rgbaChannels, element) {
        if (!rgbaChannels) {
          element.removeAttribute("lwt-toolbar-field-focus-brighttext");
          return null;
        }
        const { r, g, b, a } = rgbaChannels;
        if (_isColorDark(r, g, b)) {
          element.removeAttribute("lwt-toolbar-field-focus-brighttext");
        } else {
          element.setAttribute("lwt-toolbar-field-focus-brighttext", "true");
        }
        return `rgba(${r}, ${g}, ${b}, ${a})`;
      },
    },
  ],
  [
    "--toolbar-field-focus-border-color",
    {
      lwtProperty: "toolbar_field_border_focus",
    },
  ],
  [
    "--lwt-toolbar-field-highlight",
    {
      lwtProperty: "toolbar_field_highlight",
      processColor(rgbaChannels, element) {
        if (!rgbaChannels) {
          element.removeAttribute("lwt-selection");
          return null;
        }
        element.setAttribute("lwt-selection", "true");
        const { r, g, b, a } = rgbaChannels;
        return `rgba(${r}, ${g}, ${b}, ${a})`;
      },
    },
  ],
  [
    "--lwt-toolbar-field-highlight-text",
    {
      lwtProperty: "toolbar_field_highlight_text",
    },
  ],
];

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._win = aDocument.defaultView;
  this._winId = this._win.windowUtils.outerWindowID;

  Services.obs.addObserver(this, "lightweight-theme-styling-update");

  // In Linux, the default theme picks up the right colors from dark GTK themes.
  if (AppConstants.platform != "linux") {
    this.darkThemeMediaQuery = this._win.matchMedia("(-moz-system-dark-theme)");
    this.darkThemeMediaQuery.addListener(this);
  }

  const { LightweightThemeManager } = ChromeUtils.import(
    "resource://gre/modules/LightweightThemeManager.jsm"
  );
  this._update(LightweightThemeManager.themeData);

  this._win.addEventListener("resolutionchange", this);
  this._win.addEventListener("unload", this, { once: true });
}

LightweightThemeConsumer.prototype = {
  _lastData: null,

  observe(aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update") {
      return;
    }

    let data = aSubject.wrappedJSObject;
    if (data.window && data.window !== this._winId) {
      return;
    }

    this._update(data);
  },

  handleEvent(aEvent) {
    if (aEvent.target == this.darkThemeMediaQuery) {
      this._update(this._lastData);
      return;
    }

    switch (aEvent.type) {
      case "resolutionchange":
        this._update(this._lastData);
        break;
      case "unload":
        Services.obs.removeObserver(this, "lightweight-theme-styling-update");
        Services.ppmm.sharedData.delete(`theme/${this._winId}`);
        this._win.removeEventListener("resolutionchange", this);
        this._win = this._doc = null;
        if (this.darkThemeMediaQuery) {
          this.darkThemeMediaQuery.removeListener(this);
          this.darkThemeMediaQuery = null;
        }
        break;
    }
  },

  get darkMode() {
    return this.darkThemeMediaQuery && this.darkThemeMediaQuery.matches;
  },

  _update(themeData) {
    this._lastData = themeData;

    let theme = themeData.theme;
    if (themeData.darkTheme && this.darkMode) {
      theme = themeData.darkTheme;
    }
    if (!theme) {
      theme = { id: DEFAULT_THEME_ID };
    }

    let active = (this._active = Object.keys(theme).length);

    let root = this._doc.documentElement;

    if (active && theme.headerURL) {
      root.setAttribute("lwtheme-image", "true");
    } else {
      root.removeAttribute("lwtheme-image");
    }

    this._setExperiment(active, themeData.experiment, theme.experimental);
    _setImage(root, active, "--lwt-header-image", theme.headerURL);
    _setImage(
      root,
      active,
      "--lwt-additional-images",
      theme.additionalBackgrounds
    );
    _setProperties(root, active, theme);

    if (theme.id != DEFAULT_THEME_ID || this.darkMode) {
      root.setAttribute("lwtheme", "true");
    } else {
      root.removeAttribute("lwtheme");
      root.removeAttribute("lwthemetextcolor");
    }
    if (theme.id == DEFAULT_THEME_ID && this.darkMode) {
      root.setAttribute("lwt-default-theme-in-dark-mode", "true");
    } else {
      root.removeAttribute("lwt-default-theme-in-dark-mode");
    }

    let contentThemeData = _getContentProperties(this._doc, active, theme);
    Services.ppmm.sharedData.set(`theme/${this._winId}`, contentThemeData);

    this._win.dispatchEvent(new CustomEvent("windowlwthemeupdate"));
  },

  _setExperiment(active, experiment, properties) {
    const root = this._doc.documentElement;
    if (this._lastExperimentData) {
      const { stylesheet, usedVariables } = this._lastExperimentData;
      if (stylesheet) {
        stylesheet.remove();
      }
      if (usedVariables) {
        for (const [variable] of usedVariables) {
          _setProperty(root, false, variable);
        }
      }
    }

    this._lastExperimentData = {};

    if (!active || !experiment) {
      return;
    }

    let usedVariables = [];
    if (properties.colors) {
      for (const property in properties.colors) {
        const cssVariable = experiment.colors[property];
        const value = _sanitizeCSSColor(
          root.ownerDocument,
          properties.colors[property]
        );
        usedVariables.push([cssVariable, value]);
      }
    }

    if (properties.images) {
      for (const property in properties.images) {
        const cssVariable = experiment.images[property];
        usedVariables.push([
          cssVariable,
          `url(${properties.images[property]})`,
        ]);
      }
    }
    if (properties.properties) {
      for (const property in properties.properties) {
        const cssVariable = experiment.properties[property];
        usedVariables.push([cssVariable, properties.properties[property]]);
      }
    }
    for (const [variable, value] of usedVariables) {
      _setProperty(root, true, variable, value);
    }
    this._lastExperimentData.usedVariables = usedVariables;

    if (experiment.stylesheet) {
      /* Stylesheet URLs are validated using WebExtension schemas */
      let stylesheetAttr = `href="${experiment.stylesheet}" type="text/css"`;
      let stylesheet = this._doc.createProcessingInstruction(
        "xml-stylesheet",
        stylesheetAttr
      );
      this._doc.insertBefore(stylesheet, root);
      this._lastExperimentData.stylesheet = stylesheet;
    }
  },
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
  _setProperty(
    aRoot,
    aActive,
    aVariableName,
    aURLs && aURLs.map(v => `url("${v.replace(/"/g, '\\"')}")`).join(",")
  );
}

function _setProperty(elem, active, variableName, value) {
  if (active && value) {
    elem.style.setProperty(variableName, value);
  } else {
    elem.style.removeProperty(variableName);
  }
}

function _setProperties(root, active, themeData) {
  let properties = [];
  let propertyOverrides = new Map();

  for (let map of [toolkitVariableMap, ThemeVariableMap]) {
    for (let [cssVarName, definition] of map) {
      const {
        lwtProperty,
        fallbackProperty,
        optionalElementID,
        processColor,
        isColor = true,
      } = definition;
      let elem = optionalElementID
        ? root.ownerDocument.getElementById(optionalElementID)
        : root;
      let val = propertyOverrides.get(lwtProperty) || themeData[lwtProperty];
      if (isColor) {
        val = _sanitizeCSSColor(root.ownerDocument, val);
        if (!val && fallbackProperty) {
          val = _sanitizeCSSColor(
            root.ownerDocument,
            themeData[fallbackProperty]
          );
        }
        if (processColor) {
          val = processColor(_parseRGBA(val), elem, propertyOverrides);
        }
      }
      properties.push([elem, cssVarName, val]);
    }
  }

  // Set all the properties together, since _sanitizeCSSColor flushes.
  for (const [elem, cssVarName, val] of properties) {
    _setProperty(elem, active, cssVarName, val);
  }
}

function _sanitizeCSSColor(doc, cssColor) {
  if (!cssColor) {
    return null;
  }
  const HTML_NS = "http://www.w3.org/1999/xhtml";
  // style.color normalizes color values and makes invalid ones black, so a
  // simple round trip gets us a sanitized color value.
  // Use !important so that the theme's stylesheets cannot override us.
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.setProperty("color", "black", "important");
  div.style.setProperty("display", "none", "important");
  let span = doc.createElementNS(HTML_NS, "span");
  span.style.setProperty("color", cssColor, "important");

  // CSS variables are not allowed and should compute to black.
  if (span.style.color.includes("var(")) {
    span.style.color = "";
  }

  div.appendChild(span);
  doc.documentElement.appendChild(div);
  cssColor = doc.defaultView.getComputedStyle(span).color;
  div.remove();
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

function _isColorDark(r, g, b) {
  return 0.2125 * r + 0.7154 * g + 0.0721 * b <= 110;
}
