/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ThemeContentPropertyList: "resource:///modules/ThemeVariableMap.sys.mjs",
  ThemeVariableMap: "resource:///modules/ThemeVariableMap.sys.mjs",
});

const {AddonManager} = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "BuiltInThemeConfig",
  "resource:///modules/BuiltInThemeConfig.jsm",
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

  Services.prefs.addObserver("floorp.dualtheme.theme",this.dual_obs.bind(this))
  Services.prefs.addObserver("floorp.enable.dualtheme",this.dual_obs.bind(this))
  Services.prefs.addObserver("extensions.experiments.enabled",this.dual_obs.bind(this))

  this._win.addEventListener("unload", this, { once: true });
}

LightweightThemeConsumer.prototype = {
  _lastData: null,
  dualtheme_count_url:0,
  protocolHandler: Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler),

  dual_obs(){
    const { LightweightThemeManager } = ChromeUtils.import(
      "resource://gre/modules/LightweightThemeManager.jsm"
    );
    this._update(LightweightThemeManager.themeData);
  },

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

  async _update(themeData_) {
    let themeData
    if(Services.prefs.getBoolPref("floorp.enable.dualtheme", false)){
      themeData = JSON.parse(JSON.stringify(themeData_))
    }else{
      themeData = themeData_;
    }
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

    if(Services.prefs.getBoolPref("floorp.enable.dualtheme", false)){
      let kaonasi_dual_theme = []
      try {
        kaonasi_dual_theme = JSON.parse(Services.prefs.getStringPref("floorp.dualtheme.theme"))
      } catch (e) {
        kaonasi_dual_theme = "error"
      }
      if(kaonasi_dual_theme != "error"){
        let dualtheme_data = {}
        let dualtheme_experiment = {}
        let jsondata = {}
        let theme_path = ""
        for (let elem of kaonasi_dual_theme){
          if(lazy.BuiltInThemeConfig.has(elem)){
            theme_path = lazy.BuiltInThemeConfig.get(elem).path
            let res = await fetch(theme_path + "manifest.json")
            jsondata = await res.json()
            dualtheme_data = (useDarkTheme && !!jsondata.dark_theme) ? jsondata.dark_theme : jsondata.theme;
            dualtheme_experiment = jsondata.theme_experiment
          }
          else{
            let addon = await AddonManager.getAddonByID(elem)
            this.protocolHandler.setSubstitution("j" + this.dualtheme_count_url , Services.io.newURI(addon.getResourceURI().displaySpec, null, null));
            theme_path = `resource://j${this.dualtheme_count_url}/`
            this.dualtheme_count_url += 1
            let res = await fetch(`${theme_path}manifest.json`)
            jsondata = await res.json()
            dualtheme_data = (useDarkTheme && !!jsondata.dark_theme) ? jsondata.dark_theme : jsondata.theme;
            dualtheme_experiment = jsondata.theme_experiment
          }
          if(dualtheme_data.colors != undefined){
            for (let color of Object.keys(dualtheme_data.colors)) {
              let val = dualtheme_data.colors[color];
              if (!val) {
                continue;
              }
              let cssColor = val;
              if (Array.isArray(val)) {
                cssColor =
                    "rgb" + (val.length > 3 ? "a" : "") + "(" + val.join(",") + ")";
              }
              switch (color) {
                case "frame":
                  if (!theme.accentcolor) theme.accentcolor = cssColor;
                  break;
                case "frame_inactive":
                  if (!theme.accentcolorInactive) theme.accentcolorInactive = cssColor;
                  break;
                case "tab_background_text":
                  if (!theme.textcolor) theme.textcolor = cssColor;
                  break;
                case "toolbar":
                  if (!theme.toolbarColor) theme.toolbarColor = cssColor;
                  break;
                case "toolbar_text":
                case "bookmark_text":
                  if (!theme.toolbar_text) theme.toolbar_text = cssColor;
                  break;
                case "icons":
                  if (!theme.icon_color) theme.icon_color = cssColor;
                  break;
                case "icons_attention":
                  if (!theme.icon_attention_color) theme.icon_attention_color = cssColor;
                  break;
                case "tab_background_separator":
                case "tab_loading":
                case "tab_text":
                case "tab_line":
                case "tab_selected":
                case "toolbar_field":
                case "toolbar_field_text":
                case "toolbar_field_border":
                case "toolbar_field_focus":
                case "toolbar_field_text_focus":
                case "toolbar_field_border_focus":
                case "toolbar_top_separator":
                case "toolbar_bottom_separator":
                case "toolbar_vertical_separator":
                case "button_background_hover":
                case "button_background_active":
                case "popup":
                case "popup_text":
                case "popup_border":
                case "popup_highlight":
                case "popup_highlight_text":
                case "ntp_background":
                case "ntp_card_background":
                case "ntp_text":
                case "sidebar":
                case "sidebar_border":
                case "sidebar_text":
                case "sidebar_highlight":
                case "sidebar_highlight_text":
                case "toolbar_field_highlight":
                case "toolbar_field_highlight_text":
                  if (!theme[color]) theme[color] = cssColor;
                  break;
                default:
                  if (
                    (lazy.BuiltInThemeConfig.has(elem) || Services.prefs.getBoolPref("extensions.experiments.enabled",false)) &&
                    dualtheme_experiment &&
                    dualtheme_experiment.colors &&
                    color in dualtheme_experiment.colors
                  ) {
                    let color_experiment = color
                    if(!("experiment" in themeData && themeData != null)) themeData.experiment = {"colors":{}}
                    if(!("colors" in themeData.experiment && themeData.experiment != null && themeData.experiment.colors != null)) themeData.experiment.colors = {}
                    if("experimental" in theme && "colors" in theme.experimental){
                      while(color_experiment in themeData.experiment.colors || color_experiment in theme.experimental.colors){
                        color_experiment += "_"
                      }
                    }
                    if(!("experimental" in theme)) theme.experimental = {}
                    theme.experimental.colors = { [color_experiment]:cssColor , ...theme.experimental.colors }
                    themeData.experiment.colors[color_experiment] = dualtheme_experiment.colors[color]
                  }
                  break;
              }
            }
          }
  
          if(dualtheme_data.images != undefined){
            for (let image of Object.keys(dualtheme_data.images)) {
              let val = dualtheme_data.images[image];
              if (!val) {
                continue;
              }
              switch (image) {
                case "additional_backgrounds": {
                  let backgroundImages = val.map(img => (new URL(img.slice(0,2) != "./" ? img : "./" + img,theme_path)).href);
                  if (!theme.additionalBackgrounds) theme.additionalBackgrounds = backgroundImages;
                  break;
                }
                case "theme_frame": {
                  if(val.slice(0,2) != "./") val = "./" + val
                  let resolvedURL = (new URL(val,theme_path)).href;
                  if (!theme.headerURL) theme.headerURL = resolvedURL;
                  break;
                }
                default: {
                  if (
                    (lazy.BuiltInThemeConfig.has(elem) || Services.prefs.getBoolPref("extensions.experiments.enabled",false)) &&
                    dualtheme_experiment &&
                    dualtheme_experiment.images &&
                    image in dualtheme_experiment.images
                  ) {
                    let image_experiment = image
                    if(!("experiment" in themeData && themeData != null)) themeData.experiment = {"images":{}}
                    if(!("images" in themeData.experiment && themeData.experiment != null && themeData.experiment.images != null)) themeData.experiment.images = {}
                    if("experimental" in theme && "images" in theme.experimental){
                      while(image_experiment in themeData.experiment.images || image_experiment in theme.experimental.images){
                        image_experiment += "_"
                      }
                    }
                    if(!("experimental" in theme)) theme.experimental = {}
                    if(val.slice(0,2) != "./") val = "./" + val
                    theme.experimental.images = { [image_experiment]:(new URL(val,theme_path)).href , ...theme.experimental.images }
                    themeData.experiment.images[image_experiment] = dualtheme_experiment.images[image]
                  }
                  break;
                }
              }
            }
          }
          if(dualtheme_data.properties != undefined){
            let additionalBackgroundsCount =
              (theme.additionalBackgrounds && theme.additionalBackgrounds.length) || 0;
            const assertValidAdditionalBackgrounds = (property, valueCount) => {
              if (!additionalBackgroundsCount) {
                return false;
              }
              return true;
            }
            for (let property of Object.getOwnPropertyNames(dualtheme_data.properties)) {
              let val = dualtheme_data.properties[property];
              if (!val) {
                continue;
              }
              switch (property) {
                case "additional_backgrounds_alignment": {
                  if (!assertValidAdditionalBackgrounds(property, val.length)) {
                    break;
                  }
  
                  if (!theme.backgroundsAlignment) theme.backgroundsAlignment = val.join(",");
                  break;
                }
                case "additional_backgrounds_tiling": {
                  if (!assertValidAdditionalBackgrounds(property, val.length)) {
                    break;
                  }
                  let tiling = [];
                  for (let i = 0, l = theme.additionalBackgrounds.length; i < l; ++i) {
                    tiling.push(val[i] || "no-repeat");
                  }
                  if (!theme.backgroundsTiling) theme.backgroundsTiling = tiling.join(",");
                  break;
                }
                case "color_scheme":
                case "content_color_scheme": {
                  if (!theme[property]) theme[property] = val;
                  break;
                }
                default: {
                  if (
                    (lazy.BuiltInThemeConfig.has(elem) || Services.prefs.getBoolPref("extensions.experiments.enabled",false)) &&
                    dualtheme_experiment &&
                    dualtheme_experiment.properties &&
                    property in dualtheme_experiment.properties
                  ) {
                    let property_experiment = property
                    if(!("experiment" in themeData && themeData != null)) themeData.experiment = {"properties":{}}
                    if(!("properties" in themeData.experiment && themeData.experiment != null && themeData.experiment.properties != null)) themeData.experiment.properties = {}
                    if("experimental" in theme && "properties" in theme.experimental){
                      while(property_experiment in themeData.experiment.properties || property_experiment in theme.experimental.properties){
                        property_experiment += "_"
                      }
                    }
                    if(!("experimental" in theme)) theme.experimental = {}
                    theme.experimental.properties = { [property_experiment]:val , ...theme.experimental.properties }
                    themeData.experiment.properties[property_experiment] = dualtheme_experiment.properties[property]
                  }
                  break;
                }
              }
            }
          }
          if(dualtheme_experiment != undefined && dualtheme_experiment.stylesheet != undefined && dualtheme_experiment.stylesheet != null && (lazy.BuiltInThemeConfig.has(elem) || Services.prefs.getBoolPref("extensions.experiments.enabled",false))) {
            let val = dualtheme_experiment.stylesheet;
            if(val.slice(0,2) != "./") val = "./" + val
            if(!("experiment" in themeData && themeData != null)) themeData.experiment = {"dual_stylesheets":[]}
            if(!("dual_stylesheets" in themeData.experiment && themeData.experiment != null)) themeData.experiment.dual_stylesheets = []
            themeData.experiment.dual_stylesheets.unshift((new URL(val,theme_path)).href)
            }
          }
        }
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
      const { stylesheet, dual_stylesheets, usedVariables } = this._lastExperimentData;
      if (dual_stylesheets) {
        dual_stylesheets.forEach(elem => elem.remove());
      }
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

    if (experiment.dual_stylesheets && experiment.dual_stylesheets.length != 0) {
      for( let elem of experiment.dual_stylesheets ) {
              /* Stylesheet URLs are validated using WebExtension schemas */
      let stylesheetAttr = `href="${elem}" type="text/css"`;
      let stylesheet = this._doc.createProcessingInstruction(
        "xml-stylesheet",
        stylesheetAttr
      );
      this._doc.insertBefore(stylesheet, root);
      if(this._lastExperimentData.dual_stylesheets == undefined) this._lastExperimentData.dual_stylesheets = []
      this._lastExperimentData.dual_stylesheets.push(stylesheet)
      }
    }

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

  let toolbarTheme = (function () {
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

  let contentTheme = (function () {
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
