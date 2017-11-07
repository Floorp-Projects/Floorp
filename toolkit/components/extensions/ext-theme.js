"use strict";

/* global windowTracker, EventManager, EventEmitter */

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");

XPCOMUtils.defineLazyGetter(this, "gThemesEnabled", () => {
  return Services.prefs.getBoolPref("extensions.webextensions.themes.enabled");
});

var {
  getWinUtils,
} = ExtensionUtils;

const ICONS = Services.prefs.getStringPref("extensions.webextensions.themes.icons.buttons", "").split(",");

/** Class representing a theme. */
class Theme {
  /**
   * Creates a theme instance.
   *
   * @param {string} baseURI The base URI of the extension, used to
   *   resolve relative filepaths.
   * @param {Object} logger  Reference to the (console) logger that will be used
   *   to show manifest warnings to the theme author.
   */
  constructor(baseURI, logger) {
    // The base theme applied to all windows.
    this.baseProperties = {};

    // Window-specific theme overrides.
    this.windowOverrides = new WeakMap();

    this.baseURI = baseURI;
    this.logger = logger;
  }

  /**
   * Gets the current theme for a specified window
   *
   * @param {Object} window
   * @returns {Object} The theme of the specified window
   */
  getWindowTheme(window) {
    if (this.windowOverrides.has(window)) {
      return this.windowOverrides.get(window);
    }
    return this.baseProperties;
  }

  /**
   * Loads a theme by reading the properties from the extension's manifest.
   * This method will override any currently applied theme.
   *
   * @param {Object} details Theme part of the manifest. Supported
   *   properties can be found in the schema under ThemeType.
   * @param {Object} targetWindow The window to apply the theme to. Omitting
   *   this parameter will apply the theme globally.
   */
  load(details, targetWindow) {
    this.lwtStyles = {
      icons: {},
    };

    if (targetWindow) {
      this.lwtStyles.window = getWinUtils(targetWindow).outerWindowID;
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

    // Lightweight themes require all properties to be defined.
    if (this.lwtStyles.headerURL &&
        this.lwtStyles.accentcolor &&
        this.lwtStyles.textcolor) {
      if (!targetWindow) {
        this.baseProperties = details;
      } else {
        this.windowOverrides.set(targetWindow, details);
      }
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
        case "textcolor":
        case "tab_text":
          this.lwtStyles.textcolor = cssColor;
          break;
        case "toolbar":
          this.lwtStyles.toolbarColor = cssColor;
          break;
        case "toolbar_text":
        case "bookmark_text":
          this.lwtStyles.toolbar_text = cssColor;
          break;
        case "toolbar_field":
        case "toolbar_field_text":
        case "toolbar_top_separator":
        case "toolbar_bottom_separator":
        case "toolbar_vertical_separator":
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

  /**
   * Unloads the currently applied theme.
   * @param {Object} targetWindow The window the theme should be unloaded from
   */
  unload(targetWindow) {
    this.lwtStyles = {
      headerURL: "",
      accentcolor: "",
      additionalBackgrounds: "",
      backgroundsAlignment: "",
      backgroundsTiling: "",
      textcolor: "",
      icons: {},
    };

    if (targetWindow) {
      this.lwtStyles.window = getWinUtils(targetWindow).outerWindowID;
      this.windowOverrides.set(targetWindow, {});
    } else {
      this.windowOverrides = new WeakMap();
      this.baseProperties = {};
    }

    for (let icon of ICONS) {
      this.lwtStyles.icons[`--${icon}--icon`] = "";
    }
    LightweightThemeManager.fallbackThemeData = null;
    Services.obs.notifyObservers(null,
      "lightweight-theme-styling-update",
      JSON.stringify(this.lwtStyles));
  }
}

const onUpdatedEmitter = new EventEmitter();

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

    this.theme = new Theme(extension.baseURI, extension.logger);
    this.theme.load(manifest.theme);
    onUpdatedEmitter.emit("theme-updated", manifest.theme);
  }

  onShutdown() {
    if (this.theme) {
      this.theme.unload();
    }
    onUpdatedEmitter.emit("theme-updated", {});
  }

  getAPI(context) {
    let {extension} = context;

    return {
      theme: {
        getCurrent: (windowId) => {
          // Return empty theme if none is applied.
          if (!this.theme) {
            return Promise.resolve({});
          }

          // Return theme applied on last focused window when no ID is supplied.
          if (!windowId) {
            return Promise.resolve(this.theme.getWindowTheme(windowTracker.topWindow));
          }

          const browserWindow = windowTracker.getWindow(windowId, context);
          if (!browserWindow) {
            return Promise.reject(`Invalid window ID: ${windowId}`);
          }
          return Promise.resolve(this.theme.getWindowTheme(browserWindow));
        },
        update: (windowId, details) => {
          if (!gThemesEnabled) {
            // Return early if themes are disabled.
            return;
          }

          if (!this.theme) {
            // WebExtensions using the Theme API will not have a theme defined
            // in the manifest. Therefore, we need to initialize the theme the
            // first time browser.theme.update is called.
            this.theme = new Theme(extension.baseURI, extension.logger);
          }

          let browserWindow;
          if (windowId !== null) {
            browserWindow = windowTracker.getWindow(windowId, context);
          }
          this.theme.load(details, browserWindow);
          onUpdatedEmitter.emit("theme-updated", details, windowId);
        },
        reset: (windowId) => {
          if (!gThemesEnabled) {
            // Return early if themes are disabled.
            return;
          }

          if (!this.theme) {
            // If no theme has been initialized, nothing to do.
            return;
          }

          let browserWindow;
          if (windowId !== null) {
            browserWindow = windowTracker.getWindow(windowId, context);
          }

          this.theme.unload(browserWindow);
          onUpdatedEmitter.emit("theme-updated", {}, windowId);
        },
        onUpdated: new EventManager(context, "theme.onUpdated", fire => {
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
        }).api(),
      },
    };
  }
};
