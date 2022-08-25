/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const lazy = {};
// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineModuleGetter(
  lazy,
  "ThemeContentPropertyList",
  "resource:///modules/ThemeVariableMap.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ThemeVariableMap",
  "resource:///modules/ThemeVariableMap.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NimbusFeatures",
  "resource://nimbus/ExperimentAPI.jsm"
);

// Whether the content and chrome areas should always use the same color
// scheme (unless user-overridden). Thunderbird uses this.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BROWSER_THEME_UNIFIED_COLOR_SCHEME",
  "browser.theme.unified-color-scheme",
  false
);

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

// On Linux, the default theme picks up the right colors from dark GTK themes.
const DEFAULT_THEME_RESPECTS_SYSTEM_COLOR_SCHEME =
  AppConstants.platform == "linux";

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
        const descriptionColorVariable = "--panel-description-color";

        if (!rgbaChannels) {
          element.style.removeProperty(disabledColorVariable);
          element.style.removeProperty(descriptionColorVariable);
          return null;
        }

        let { r, g, b, a } = rgbaChannels;

        element.style.setProperty(
          disabledColorVariable,
          `rgba(${r}, ${g}, ${b}, 0.5)`
        );
        element.style.setProperty(
          descriptionColorVariable,
          `rgba(${r}, ${g}, ${b}, 0.7)`
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
    "--toolbar-field-background-color",
    {
      lwtProperty: "toolbar_field",
    },
  ],
  [
    "--toolbar-field-color",
    {
      lwtProperty: "toolbar_field_text",
    },
  ],
  [
    "--toolbar-field-border-color",
    {
      lwtProperty: "toolbar_field_border",
    },
  ],
  [
    "--toolbar-field-focus-background-color",
    {
      lwtProperty: "toolbar_field_focus",
      fallbackProperty: "toolbar_field",
      processColor(rgbaChannels, element, propertyOverrides) {
        if (!rgbaChannels) {
          return null;
        }
        // Ensure minimum opacity as this is used behind address bar results.
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
    "--toolbar-field-focus-color",
    {
      lwtProperty: "toolbar_field_text_focus",
      fallbackProperty: "toolbar_field_text",
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
          return null;
        }
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
  this._winId = this._win.docShell.outerWindowID;

  Services.obs.addObserver(this, "lightweight-theme-styling-update");

  this.darkThemeMediaQuery = this._win.matchMedia("(-moz-system-dark-theme)");
  this.darkThemeMediaQuery.addListener(this);

  const { LightweightThemeManager } = ChromeUtils.import(
    "resource://gre/modules/LightweightThemeManager.jsm"
  );
  this._update(LightweightThemeManager.themeData);

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
      case "unload":
        Services.obs.removeObserver(this, "lightweight-theme-styling-update");
        Services.ppmm.sharedData.delete(`theme/${this._winId}`);
        this._win = this._doc = null;
        if (this.darkThemeMediaQuery) {
          this.darkThemeMediaQuery.removeListener(this);
          this.darkThemeMediaQuery = null;
        }
        break;
    }
  },

  _update(themeData) {
    this._lastData = themeData;

    const hasDarkTheme = !!themeData.darkTheme;
    let updateGlobalThemeData = true;
    let useDarkTheme = (() => {
      if (!hasDarkTheme) {
        return false;
      }
      if (this.darkThemeMediaQuery?.matches) {
        return (
          themeData.darkTheme.id != DEFAULT_THEME_ID ||
          !DEFAULT_THEME_RESPECTS_SYSTEM_COLOR_SCHEME
        );
      }

      // If enabled, apply the dark theme variant to private browsing windows.
      if (
        !lazy.NimbusFeatures.majorRelease2022.getVariable(
          "feltPrivacyPBMDarkTheme"
        ) ||
        !lazy.PrivateBrowsingUtils.isWindowPrivate(this._win) ||
        lazy.PrivateBrowsingUtils.permanentPrivateBrowsing
      ) {
        return false;
      }
      // When applying the dark theme for a PBM window we need to skip calling
      // _determineToolbarAndContentTheme, because it applies the color scheme
      // globally for all windows. Skipping this method also means we don't
      // switch the content theme to dark.
      updateGlobalThemeData = false;
      return true;
    })();

    // If this is a per-window dark theme, set the color scheme override so
    // child BrowsingContexts, such as embedded prompts, get themed
    // appropriately.
    // If not, reset the color scheme override field. This is required to reset
    // the color scheme on theme switch.
    if (this._win.browsingContext == this._win.browsingContext.top) {
      if (useDarkTheme && !updateGlobalThemeData) {
        this._win.browsingContext.prefersColorSchemeOverride = "dark";
      } else {
        this._win.browsingContext.prefersColorSchemeOverride = "none";
      }
    }

    let theme = useDarkTheme ? themeData.darkTheme : themeData.theme;
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
    _setImage(this._win, root, active, "--lwt-header-image", theme.headerURL);
    _setImage(
      this._win,
      root,
      active,
      "--lwt-additional-images",
      theme.additionalBackgrounds
    );
    _setProperties(root, active, theme);

    if (theme.id != DEFAULT_THEME_ID || useDarkTheme) {
      if (updateGlobalThemeData) {
        _determineToolbarAndContentTheme(
          this._doc,
          theme,
          hasDarkTheme,
          useDarkTheme
        );
      }
      root.setAttribute("lwtheme", "true");
    } else {
      _determineToolbarAndContentTheme(this._doc, null);
      root.removeAttribute("lwtheme");
    }
    if (theme.id == DEFAULT_THEME_ID && useDarkTheme) {
      root.setAttribute("lwt-default-theme-in-dark-mode", "true");
    } else {
      root.removeAttribute("lwt-default-theme-in-dark-mode");
    }

    _setDarkModeAttributes(this._doc, root, theme._processedColors);

    let contentThemeData = _getContentProperties(this._doc, active, theme);
    Services.ppmm.sharedData.set(`theme/${this._winId}`, contentThemeData);
    // We flush sharedData because contentThemeData can be responsible for
    // painting large background surfaces. If this data isn't delivered to the
    // content process before about:home is painted, we will paint a default
    // background and then replace it when sharedData syncs, causing flashing.
    Services.ppmm.sharedData.flush();

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
        const value = _rgbaToString(
          _cssColorToRGBA(root.ownerDocument, properties.colors[property])
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
    if (lazy.ThemeContentPropertyList.includes(property)) {
      properties[property] = _cssColorToRGBA(doc, data[property]);
    }
  }
  return properties;
}

function _setImage(aWin, aRoot, aActive, aVariableName, aURLs) {
  if (aURLs && !Array.isArray(aURLs)) {
    aURLs = [aURLs];
  }
  _setProperty(
    aRoot,
    aActive,
    aVariableName,
    aURLs && aURLs.map(v => `url(${aWin.CSS.escape(v)})`).join(", ")
  );
}

function _setProperty(elem, active, variableName, value) {
  if (active && value) {
    elem.style.setProperty(variableName, value);
  } else {
    elem.style.removeProperty(variableName);
  }
}

function _determineToolbarAndContentTheme(
  aDoc,
  aTheme,
  aHasDarkTheme = false,
  aIsDarkTheme = false
) {
  const kDark = 0;
  const kLight = 1;
  const kSystem = 2;

  const colors = aTheme?._processedColors;
  function prefValue(aColor, aIsForeground = false) {
    if (typeof aColor != "object") {
      aColor = _cssColorToRGBA(aDoc, aColor);
    }
    return _isColorDark(aColor.r, aColor.g, aColor.b) == aIsForeground
      ? kLight
      : kDark;
  }

  function colorSchemeValue(aColorScheme) {
    if (!aColorScheme) {
      return null;
    }
    switch (aColorScheme) {
      case "light":
        return kLight;
      case "dark":
        return kDark;
      case "system":
        return kSystem;
      case "auto":
      default:
        break;
    }
    return null;
  }

  let toolbarTheme = (function() {
    if (!aTheme) {
      if (!DEFAULT_THEME_RESPECTS_SYSTEM_COLOR_SCHEME) {
        return kLight;
      }
      return kSystem;
    }
    let themeValue = colorSchemeValue(aTheme.color_scheme);
    if (themeValue !== null) {
      return themeValue;
    }
    if (aHasDarkTheme) {
      return aIsDarkTheme ? kDark : kLight;
    }
    // We prefer looking at toolbar background first (if it's opaque) because
    // some text colors can be dark enough for our heuristics, but still
    // contrast well enough with a dark background, see bug 1743010.
    if (colors.toolbarColor) {
      let color = _cssColorToRGBA(aDoc, colors.toolbarColor);
      if (color.a == 1) {
        return prefValue(color);
      }
    }
    if (colors.toolbar_text) {
      return prefValue(colors.toolbar_text, /* aIsForeground = */ true);
    }
    // It'd seem sensible to try looking at the "frame" background (accentcolor),
    // but we don't because some themes that use background images leave it to
    // black, see bug 1741931.
    //
    // Fall back to black as per the textcolor processing above.
    return prefValue(colors.textcolor || "black", /* aIsForeground = */ true);
  })();

  let contentTheme = (function() {
    if (lazy.BROWSER_THEME_UNIFIED_COLOR_SCHEME) {
      return toolbarTheme;
    }
    if (!aTheme) {
      if (!DEFAULT_THEME_RESPECTS_SYSTEM_COLOR_SCHEME) {
        return kLight;
      }
      return kSystem;
    }
    let themeValue = colorSchemeValue(
      aTheme.content_color_scheme || aTheme.color_scheme
    );
    if (themeValue !== null) {
      return themeValue;
    }
    return kSystem;
  })();

  Services.prefs.setIntPref("browser.theme.toolbar-theme", toolbarTheme);
  Services.prefs.setIntPref("browser.theme.content-theme", contentTheme);
}

/**
 * Sets dark mode attributes on root, if required. We must do this here,
 * instead of in each color's processColor function, because multiple colors
 * are considered.
 * @param {Document} doc
 * @param {Element} root
 * @param {object} colors
 *   The `_processedColors` object from the object created for our theme.
 */
function _setDarkModeAttributes(doc, root, colors) {
  {
    let textColor = _cssColorToRGBA(doc, colors.textcolor);
    if (textColor && !_isColorDark(textColor.r, textColor.g, textColor.b)) {
      root.setAttribute("lwtheme-brighttext", "true");
    } else {
      root.removeAttribute("lwtheme-brighttext");
    }
  }

  if (
    _determineIfColorPairIsDark(
      doc,
      colors,
      "toolbar_field_text",
      "toolbar_field"
    )
  ) {
    root.setAttribute("lwt-toolbar-field-brighttext", "true");
  } else {
    root.removeAttribute("lwt-toolbar-field-brighttext");
  }

  if (
    _determineIfColorPairIsDark(
      doc,
      colors,
      "toolbar_field_text_focus",
      "toolbar_field_focus"
    )
  ) {
    root.setAttribute("lwt-toolbar-field-focus-brighttext", "true");
  } else {
    root.removeAttribute("lwt-toolbar-field-focus-brighttext");
  }

  if (_determineIfColorPairIsDark(doc, colors, "popup_text", "popup")) {
    root.setAttribute("lwt-popup-brighttext", "true");
  } else {
    root.removeAttribute("lwt-popup-brighttext");
  }
}

/**
 * Determines if a themed color pair should be considered to have a dark color
 * scheme. We consider both the background and foreground (i.e. usually text)
 * colors because some text colors can be dark enough for our heuristics, but
 * still contrast well enough with a dark background
 * @param {Document} doc
 * @param {object} colors
 * @param {string} foregroundElementId
 *   The key for the foreground element in `colors`.
 * @param {string} backgroundElementId
 *   The key for the background element in `colors`.
 * @returns {boolean} True if the element should be considered dark, false
 *   otherwise.
 */
function _determineIfColorPairIsDark(
  doc,
  colors,
  textPropertyName,
  backgroundPropertyName
) {
  if (!colors[backgroundPropertyName] && !colors[textPropertyName]) {
    // Handles the system theme.
    return false;
  }

  let color = _cssColorToRGBA(doc, colors[backgroundPropertyName]);
  if (color && color.a == 1) {
    return _isColorDark(color.r, color.g, color.b);
  }

  color = _cssColorToRGBA(doc, colors[textPropertyName]);
  if (!color) {
    // Handles the case where a theme only provides a background color and it is
    // semi-transparent.
    return false;
  }

  return !_isColorDark(color.r, color.g, color.b);
}

function _setProperties(root, active, themeData) {
  let propertyOverrides = new Map();
  let doc = root.ownerDocument;

  // Copy the theme into _processedColors. We'll replace values with processed
  // colors if necessary. We copy because some colors (such as those used in
  // content) are not processed here, but are referenced in places that check
  // _processedColors. Copying means _processedColors will contain irrelevant
  // properties like `id`. There aren't too many, so that's OK.
  themeData._processedColors = { ...themeData };
  for (let map of [toolkitVariableMap, lazy.ThemeVariableMap]) {
    for (let [cssVarName, definition] of map) {
      const {
        lwtProperty,
        fallbackProperty,
        optionalElementID,
        processColor,
        isColor = true,
      } = definition;
      let elem = optionalElementID
        ? doc.getElementById(optionalElementID)
        : root;
      let val = propertyOverrides.get(lwtProperty) || themeData[lwtProperty];
      if (isColor) {
        val = _cssColorToRGBA(doc, val);
        if (!val && fallbackProperty) {
          val = _cssColorToRGBA(doc, themeData[fallbackProperty]);
        }
        if (processColor) {
          val = processColor(val, elem, propertyOverrides);
        } else {
          val = _rgbaToString(val);
        }
      }

      // Add processed color to themeData.
      themeData._processedColors[lwtProperty] = val;

      _setProperty(elem, active, cssVarName, val);
    }
  }
}

const kInvalidColor = { r: 0, g: 0, b: 0, a: 1 };

function _cssColorToRGBA(doc, cssColor) {
  if (!cssColor) {
    return null;
  }
  return (
    doc.defaultView.InspectorUtils.colorToRGBA(cssColor, doc) || kInvalidColor
  );
}

function _rgbaToString(parsedColor) {
  if (!parsedColor) {
    return null;
  }
  let { r, g, b, a } = parsedColor;
  if (a == 1) {
    return `rgb(${r}, ${g}, ${b})`;
  }
  return `rgba(${r}, ${g}, ${b}, ${a})`;
}

function _isColorDark(r, g, b) {
  return 0.2125 * r + 0.7154 * g + 0.0721 * b <= 127;
}
