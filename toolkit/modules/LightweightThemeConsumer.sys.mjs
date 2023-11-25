/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ThemeContentPropertyList: "resource:///modules/ThemeVariableMap.sys.mjs",
  ThemeVariableMap: "resource:///modules/ThemeVariableMap.sys.mjs",
});

// Whether the content and chrome areas should always use the same color
// scheme (unless user-overridden). Thunderbird uses this.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BROWSER_THEME_UNIFIED_COLOR_SCHEME",
  "browser.theme.unified-color-scheme",
  false
);

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

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
      fallbackColor: "rgba(255, 255, 255, 0.8)",
    },
  ],
  [
    "--toolbar-bgcolor",
    {
      lwtProperty: "toolbarColor",
    },
  ],
  [
    "--toolbar-color",
    {
      lwtProperty: "toolbar_text",
    },
  ],
  [
    "--toolbar-field-color",
    {
      lwtProperty: "toolbar_field_text",
      fallbackColor: "black",
    },
  ],
  [
    "--toolbar-field-border-color",
    {
      lwtProperty: "toolbar_field_border",
      fallbackColor: "transparent",
    },
  ],
  [
    "--toolbar-field-focus-background-color",
    {
      lwtProperty: "toolbar_field_focus",
      fallbackProperty: "toolbar_field",
      fallbackColor: "white",
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
      fallbackColor: "black",
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
  // The following 3 are given to the new tab page by contentTheme.js. They are
  // also exposed here, in the browser chrome, so popups anchored on top of the
  // new tab page can use them to avoid clashing with the new tab page content.
  [
    "--newtab-background-color",
    {
      lwtProperty: "ntp_background",
      processColor(rgbaChannels) {
        if (!rgbaChannels) {
          return null;
        }
        const { r, g, b } = rgbaChannels;
        // Drop alpha channel
        return `rgb(${r}, ${g}, ${b})`;
      },
    },
  ],
  [
    "--newtab-background-color-secondary",
    { lwtProperty: "ntp_card_background" },
  ],
  ["--newtab-text-primary-color", { lwtProperty: "ntp_text" }],
];

export function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._win = aDocument.defaultView;
  this._winId = this._win.docShell.outerWindowID;

  Services.obs.addObserver(this, "lightweight-theme-styling-update");

  this.darkThemeMediaQuery = this._win.matchMedia("(-moz-system-dark-theme)");
  this.darkThemeMediaQuery.addListener(this);

  const { LightweightThemeManager } = ChromeUtils.importESModule(
    "resource://gre/modules/LightweightThemeManager.sys.mjs"
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
        return themeData.darkTheme.id != DEFAULT_THEME_ID;
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
      //
      // TODO: On Linux we most likely need to apply the dark theme, but on
      // Windows and macOS we should be able to render light and dark windows
      // with the default theme at the same time.
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

    let hasTheme = theme.id != DEFAULT_THEME_ID || useDarkTheme;

    this._setExperiment(active, themeData.experiment, theme.experimental);
    _setImage(this._win, root, active, "--lwt-header-image", theme.headerURL);
    _setImage(
      this._win,
      root,
      active,
      "--lwt-additional-images",
      theme.additionalBackgrounds
    );
    _setProperties(root, active, theme, hasTheme);

    if (hasTheme) {
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

    _setDarkModeAttributes(this._doc, root, theme._processedColors, hasTheme);

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
  if (data.experimental) {
    for (const property in data.experimental.colors) {
      if (lazy.ThemeContentPropertyList.includes(property)) {
        properties[property] = _cssColorToRGBA(
          doc,
          data.experimental.colors[property]
        );
      }
    }
    for (const property in data.experimental.images) {
      if (lazy.ThemeContentPropertyList.includes(property)) {
        properties[property] = `url(${data.experimental.images[property]})`;
      }
    }
    for (const property in data.experimental.properties) {
      if (lazy.ThemeContentPropertyList.includes(property)) {
        properties[property] = data.experimental.properties[property];
      }
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

function _isToolbarDark(aDoc, aColors) {
  // We prefer looking at toolbar background first (if it's opaque) because
  // some text colors can be dark enough for our heuristics, but still
  // contrast well enough with a dark background, see bug 1743010.
  if (aColors.toolbarColor) {
    let color = _cssColorToRGBA(aDoc, aColors.toolbarColor);
    if (color.a == 1) {
      return _isColorDark(color.r, color.g, color.b);
    }
  }
  if (aColors.toolbar_text) {
    let color = _cssColorToRGBA(aDoc, aColors.toolbar_text);
    return !_isColorDark(color.r, color.g, color.b);
  }
  // It'd seem sensible to try looking at the "frame" background (accentcolor),
  // but we don't because some themes that use background images leave it to
  // black, see bug 1741931.
  //
  // Fall back to black as per the textcolor processing above.
  let color = _cssColorToRGBA(aDoc, aColors.textcolor || "black");
  return !_isColorDark(color.r, color.g, color.b);
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

  let toolbarTheme = (function () {
    if (!aTheme) {
      return kSystem;
    }
    let themeValue = colorSchemeValue(aTheme.color_scheme);
    if (themeValue !== null) {
      return themeValue;
    }
    if (aHasDarkTheme) {
      return aIsDarkTheme ? kDark : kLight;
    }
    return _isToolbarDark(aDoc, colors) ? kDark : kLight;
  })();

  let contentTheme = (function () {
    if (lazy.BROWSER_THEME_UNIFIED_COLOR_SCHEME) {
      return toolbarTheme;
    }
    if (!aTheme) {
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
 * @param {boolean} hasTheme
 */
function _setDarkModeAttributes(doc, root, colors, hasTheme) {
  {
    let textColor = _cssColorToRGBA(doc, colors.textcolor);
    if (textColor && !_isColorDark(textColor.r, textColor.g, textColor.b)) {
      root.setAttribute("lwtheme-brighttext", "true");
    } else {
      root.removeAttribute("lwtheme-brighttext");
    }
  }

  if (hasTheme) {
    root.setAttribute(
      "lwt-toolbar",
      _isToolbarDark(doc, colors) ? "dark" : "light"
    );
  } else {
    root.removeAttribute("lwt-toolbar");
  }

  const setAttribute = function (
    attribute,
    textPropertyName,
    backgroundPropertyName
  ) {
    let dark = _determineIfColorPairIsDark(
      doc,
      colors,
      textPropertyName,
      backgroundPropertyName
    );
    if (dark === null) {
      root.removeAttribute(attribute);
    } else {
      root.setAttribute(attribute, dark ? "dark" : "light");
    }
  };

  setAttribute("lwt-tab-selected", "tab_text", "tab_selected");
  setAttribute("lwt-toolbar-field", "toolbar_field_text", "toolbar_field");
  setAttribute(
    "lwt-toolbar-field-focus",
    "toolbar_field_text_focus",
    "toolbar_field_focus"
  );
  setAttribute("lwt-popup", "popup_text", "popup");
  setAttribute("lwt-sidebar", "sidebar_text", "sidebar");
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
 * @returns {boolean | null} True if the element should be considered dark, false
 *   if light, null for preferred scheme.
 */
function _determineIfColorPairIsDark(
  doc,
  colors,
  textPropertyName,
  backgroundPropertyName
) {
  if (!colors[backgroundPropertyName] && !colors[textPropertyName]) {
    // Handles the system theme.
    return null;
  }

  let color = _cssColorToRGBA(doc, colors[backgroundPropertyName]);
  if (color && color.a == 1) {
    return _isColorDark(color.r, color.g, color.b);
  }

  color = _cssColorToRGBA(doc, colors[textPropertyName]);
  if (!color) {
    // Handles the case where a theme only provides a background color and it is
    // semi-transparent.
    return null;
  }

  return !_isColorDark(color.r, color.g, color.b);
}

function _setProperties(root, active, themeData, hasTheme) {
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
        fallbackColor,
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
        if (!val && hasTheme && fallbackColor) {
          val = _cssColorToRGBA(doc, fallbackColor);
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
