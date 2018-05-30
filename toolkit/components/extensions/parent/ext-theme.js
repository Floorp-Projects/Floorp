"use strict";

/* global windowTracker, EventManager, EventEmitter */

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "LightweightThemeManager",
                               "resource://gre/modules/LightweightThemeManager.jsm");

XPCOMUtils.defineLazyGetter(this, "gThemesEnabled", () => {
  return Services.prefs.getBoolPref("extensions.webextensions.themes.enabled");
});

var {
  getWinUtils,
} = ExtensionUtils;

const ICONS = Services.prefs.getStringPref("extensions.webextensions.themes.icons.buttons", "").split(",");

const onUpdatedEmitter = new EventEmitter();

// Represents an empty theme for convenience of use
const emptyTheme = {
  details: {},
};


let defaultTheme = emptyTheme;
// Map[windowId -> Theme instance]
let windowOverrides = new Map();

/**
 * Class representing either a global theme affecting all windows or an override on a specific window.
 * Any extension updating the theme with a new global theme will replace the singleton defaultTheme.
 */
class Theme {
  /**
   * Creates a theme instance.
   *
   * @param {string} extension Extension that created the theme.
   * @param {Integer} windowId The windowId where the theme is applied.
   */
  constructor(extension, windowId) {
    // The base URI of the extension, used to resolve relative filepaths.
    this.baseURI = extension.baseURI;
    // Logger that will be used to show manifest warnings to the theme author.
    this.logger = extension.logger;

    this.extension = extension;
    this.windowId = windowId;
    this.lwtStyles = {
      icons: {},
    };
  }

  /**
   * Loads a theme by reading the properties from the extension's manifest.
   * This method will override any currently applied theme.
   *
   * @param {Object} details Theme part of the manifest. Supported
   *   properties can be found in the schema under ThemeType.
   */
  load(details) {
    this.details = details;

    if (this.windowId) {
      this.lwtStyles.window = getWinUtils(
        windowTracker.getWindow(this.windowId)).outerWindowID;
    }

    if (details.colors) {
      this.loadColors(details.colors);
    }

    if (details.images) {
      this.loadImages(details.images);
    }

    if (details.icons) {
      this.loadIcons(details.icons);
    }

    if (details.properties) {
      this.loadProperties(details.properties);
    }

    // Lightweight themes require accentcolor and textcolor to be defined.
    if (this.lwtStyles.accentcolor &&
        this.lwtStyles.textcolor) {
      if (this.windowId) {
        windowOverrides.set(this.windowId, this);
      } else {
        windowOverrides.clear();
        defaultTheme = this;
      }
      onUpdatedEmitter.emit("theme-updated", this.details, this.windowId);

      LightweightThemeManager.fallbackThemeData = this.lwtStyles;
      Services.obs.notifyObservers(null,
                                   "lightweight-theme-styling-update",
                                   JSON.stringify(this.lwtStyles));
    } else {
      this.logger.warn("Your theme doesn't include one of the following required " +
        "properties: 'headerURL', 'accentcolor' or 'textcolor'");
    }
  }

  /**
   * Helper method for loading colors found in the extension's manifest.
   *
   * @param {Object} colors Dictionary mapping color properties to values.
   */
  loadColors(colors) {
    for (let color of Object.keys(colors)) {
      let val = colors[color];

      if (!val) {
        continue;
      }

      let cssColor = val;
      if (Array.isArray(val)) {
        cssColor = "rgb" + (val.length > 3 ? "a" : "") + "(" + val.join(",") + ")";
      }

      switch (color) {
        case "accentcolor":
        case "frame":
          this.lwtStyles.accentcolor = cssColor;
          break;
        case "frame_inactive":
          this.lwtStyles.accentcolorInactive = cssColor;
          break;
        case "textcolor":
        case "tab_background_text":
          this.lwtStyles.textcolor = cssColor;
          break;
        case "toolbar":
          this.lwtStyles.toolbarColor = cssColor;
          break;
        case "toolbar_text":
        case "bookmark_text":
          this.lwtStyles.toolbar_text = cssColor;
          break;
        case "icons":
          this.lwtStyles.icon_color = cssColor;
          break;
        case "icons_attention":
          this.lwtStyles.icon_attention_color = cssColor;
          break;
        case "tab_background_separator":
        case "tab_loading":
        case "tab_text":
        case "tab_line":
        case "tab_selected":
        case "toolbar_field":
        case "toolbar_field_text":
        case "toolbar_field_border":
        case "toolbar_field_separator":
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
          this.lwtStyles[color] = cssColor;
          break;
      }
    }
  }

  /**
   * Helper method for loading images found in the extension's manifest.
   *
   * @param {Object} images Dictionary mapping image properties to values.
   */
  loadImages(images) {
    for (let image of Object.keys(images)) {
      let val = images[image];

      if (!val) {
        continue;
      }

      switch (image) {
        case "additional_backgrounds": {
          let backgroundImages = val.map(img => this.baseURI.resolve(img));
          this.lwtStyles.additionalBackgrounds = backgroundImages;
          break;
        }
        case "headerURL":
        case "theme_frame": {
          let resolvedURL = this.baseURI.resolve(val);
          this.lwtStyles.headerURL = resolvedURL;
          break;
        }
      }
    }
  }

  /**
   * Helper method for loading icons found in the extension's manifest.
   *
   * @param {Object} icons Dictionary mapping icon properties to extension URLs.
   */
  loadIcons(icons) {
    if (!Services.prefs.getBoolPref("extensions.webextensions.themes.icons.enabled")) {
      // Return early if icons are disabled.
      return;
    }

    for (let icon of Object.getOwnPropertyNames(icons)) {
      let val = icons[icon];
      // We also have to compare against the baseURI spec because
      // `val` might have been resolved already. Resolving "" against
      // the baseURI just produces that URI, so check for equality.
      if (!val || val == this.baseURI.spec || !ICONS.includes(icon)) {
        continue;
      }
      let variableName = `--${icon}-icon`;
      let resolvedURL = this.baseURI.resolve(val);
      this.lwtStyles.icons[variableName] = resolvedURL;
    }
  }

  /**
   * Helper method for preparing properties found in the extension's manifest.
   * Properties are commonly used to specify more advanced behavior of colors,
   * images or icons.
   *
   * @param {Object} properties Dictionary mapping properties to values.
   */
  loadProperties(properties) {
    let additionalBackgroundsCount = (this.lwtStyles.additionalBackgrounds &&
      this.lwtStyles.additionalBackgrounds.length) || 0;
    const assertValidAdditionalBackgrounds = (property, valueCount) => {
      if (!additionalBackgroundsCount) {
        this.logger.warn(`The '${property}' property takes effect only when one ` +
          `or more additional background images are specified using the 'additional_backgrounds' property.`);
        return false;
      }
      if (additionalBackgroundsCount !== valueCount) {
        this.logger.warn(`The amount of values specified for '${property}' ` +
          `(${valueCount}) is not equal to the amount of additional background ` +
          `images (${additionalBackgroundsCount}), which may lead to unexpected results.`);
      }
      return true;
    };

    for (let property of Object.getOwnPropertyNames(properties)) {
      let val = properties[property];

      if (!val) {
        continue;
      }

      switch (property) {
        case "additional_backgrounds_alignment": {
          if (!assertValidAdditionalBackgrounds(property, val.length)) {
            break;
          }

          let alignment = [];
          if (this.lwtStyles.headerURL) {
            alignment.push("right top");
          }
          this.lwtStyles.backgroundsAlignment = alignment.concat(val).join(",");
          break;
        }
        case "additional_backgrounds_tiling": {
          if (!assertValidAdditionalBackgrounds(property, val.length)) {
            break;
          }

          let tiling = [];
          if (this.lwtStyles.headerURL) {
            tiling.push("no-repeat");
          }
          for (let i = 0, l = this.lwtStyles.additionalBackgrounds.length; i < l; ++i) {
            tiling.push(val[i] || "no-repeat");
          }
          this.lwtStyles.backgroundsTiling = tiling.join(",");
          break;
        }
      }
    }
  }

  static unload(windowId) {
    let lwtStyles = {
      headerURL: "",
      accentcolor: "",
      accentcolorInactive: "",
      additionalBackgrounds: "",
      backgroundsAlignment: "",
      backgroundsTiling: "",
      textcolor: "",
      icons: {},
    };

    if (windowId) {
      lwtStyles.window = getWinUtils(windowTracker.getWindow(windowId)).outerWindowID;
      windowOverrides.set(windowId, emptyTheme);
    } else {
      windowOverrides.clear();
      defaultTheme = emptyTheme;
    }
    onUpdatedEmitter.emit("theme-updated", {}, windowId);

    for (let icon of ICONS) {
      lwtStyles.icons[`--${icon}--icon`] = "";
    }
    LightweightThemeManager.fallbackThemeData = null;
    Services.obs.notifyObservers(null,
                                 "lightweight-theme-styling-update",
                                 JSON.stringify(lwtStyles));
  }
}

this.theme = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    if (!gThemesEnabled) {
      // Return early if themes are disabled.
      return;
    }

    let {extension} = this;
    let {manifest} = extension;

    if (!gThemesEnabled) {
      // Return early if themes are disabled.
      return;
    }

    defaultTheme = new Theme(extension);
    defaultTheme.load(manifest.theme);
  }

  onShutdown(reason) {
    if (reason === "APP_SHUTDOWN") {
      return;
    }

    let {extension} = this;
    for (let [windowId, theme] of windowOverrides) {
      if (theme.extension === extension) {
        Theme.unload(windowId);
      }
    }

    if (defaultTheme.extension === extension) {
      Theme.unload();
    }
  }

  getAPI(context) {
    let {extension} = context;

    return {
      theme: {
        getCurrent: (windowId) => {
          // Take last focused window when no ID is supplied.
          if (!windowId) {
            windowId = windowTracker.getId(windowTracker.topWindow);
          }

          if (windowOverrides.has(windowId)) {
            return Promise.resolve(windowOverrides.get(windowId).details);
          }
          return Promise.resolve(defaultTheme.details);
        },
        update: (windowId, details) => {
          if (!gThemesEnabled) {
            // Return early if themes are disabled.
            return;
          }

          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }
          }

          let theme = new Theme(extension, windowId);
          theme.load(details);
        },
        reset: (windowId) => {
          if (!gThemesEnabled) {
            // Return early if themes are disabled.
            return;
          }

          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }
          }

          if (!defaultTheme && !windowOverrides.has(windowId)) {
            // If no theme has been initialized, nothing to do.
            return;
          }

          Theme.unload(windowId);
        },
        onUpdated: new EventManager({
          context,
          name: "theme.onUpdated",
          register: fire => {
            let callback = (event, theme, windowId) => {
              if (windowId) {
                fire.async({theme, windowId});
              } else {
                fire.async({theme});
              }
            };

            onUpdatedEmitter.on("theme-updated", callback);
            return () => {
              onUpdatedEmitter.off("theme-updated", callback);
            };
          },
        }).api(),
      },
    };
  }
};
