/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global windowTracker, EventManager, EventEmitter */

/* eslint-disable complexity */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "LightweightThemeManager",
  "resource://gre/modules/LightweightThemeManager.jsm"
);

var { getWinUtils } = ExtensionUtils;

const onUpdatedEmitter = new EventEmitter();

// Represents an empty theme for convenience of use
const emptyTheme = {
  details: { colors: null, images: null, properties: null },
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
  constructor({
    extension,
    details,
    darkDetails,
    windowId,
    experiment,
    startupData,
  }) {
    this.extension = extension;
    this.details = details;
    this.darkDetails = darkDetails;
    this.windowId = windowId;

    if (startupData && startupData.lwtData) {
      Object.assign(this, startupData);
    } else {
      // TODO(ntim): clean this in bug 1550090
      this.lwtStyles = {};
      this.lwtDarkStyles = null;
      if (darkDetails) {
        this.lwtDarkStyles = {};
      }

      if (experiment) {
        if (extension.experimentsAllowed) {
          this.lwtStyles.experimental = {
            colors: {},
            images: {},
            properties: {},
          };
          const { baseURI } = this.extension;
          if (experiment.stylesheet) {
            experiment.stylesheet = baseURI.resolve(experiment.stylesheet);
          }
          this.experiment = experiment;
        } else {
          const { logger } = this.extension;
          logger.warn("This extension is not allowed to run theme experiments");
          return;
        }
      }
    }
    this.load();
  }

  /**
   * Loads a theme by reading the properties from the extension's manifest.
   * This method will override any currently applied theme.
   *
   * @param {Object} details Theme part of the manifest. Supported
   *   properties can be found in the schema under ThemeType.
   */
  load() {
    if (!this.lwtData) {
      this.loadDetails(this.details, this.lwtStyles);
      if (this.darkDetails) {
        this.loadDetails(this.darkDetails, this.lwtDarkStyles);
      }

      this.lwtData = {
        theme: this.lwtStyles,
        darkTheme: this.lwtDarkStyles,
      };

      if (this.experiment) {
        this.lwtData.experiment = this.experiment;
      }

      this.extension.startupData = {
        lwtData: this.lwtData,
        lwtStyles: this.lwtStyles,
        lwtDarkStyles: this.lwtDarkStyles,
        experiment: this.experiment,
      };
      this.extension.saveStartupData();
    }

    if (this.windowId) {
      this.lwtData.window = getWinUtils(
        windowTracker.getWindow(this.windowId)
      ).outerWindowID;
      windowOverrides.set(this.windowId, this);
    } else {
      windowOverrides.clear();
      defaultTheme = this;
      LightweightThemeManager.fallbackThemeData = this.lwtData;
    }
    onUpdatedEmitter.emit("theme-updated", this.details, this.windowId);

    Services.obs.notifyObservers(
      this.lwtData,
      "lightweight-theme-styling-update"
    );
  }

  /**
   * @param {Object} details Details
   * @param {Object} styles Styles object in which to store the colors.
   */
  loadDetails(details, styles) {
    if (details.colors) {
      this.loadColors(details.colors, styles);
    }

    if (details.images) {
      this.loadImages(details.images, styles);
    }

    if (details.properties) {
      this.loadProperties(details.properties, styles);
    }

    this.loadMetadata(this.extension, styles);
  }

  /**
   * Helper method for loading colors found in the extension's manifest.
   *
   * @param {Object} colors Dictionary mapping color properties to values.
   * @param {Object} styles Styles object in which to store the colors.
   */
  loadColors(colors, styles) {
    for (let color of Object.keys(colors)) {
      let val = colors[color];

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
          styles.accentcolor = cssColor;
          break;
        case "frame_inactive":
          styles.accentcolorInactive = cssColor;
          break;
        case "tab_background_text":
          styles.textcolor = cssColor;
          break;
        case "toolbar":
          styles.toolbarColor = cssColor;
          break;
        case "toolbar_text":
        case "bookmark_text":
          styles.toolbar_text = cssColor;
          break;
        case "icons":
          styles.icon_color = cssColor;
          break;
        case "icons_attention":
          styles.icon_attention_color = cssColor;
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
        case "ntp_background":
        case "ntp_text":
        case "sidebar":
        case "sidebar_border":
        case "sidebar_text":
        case "sidebar_highlight":
        case "sidebar_highlight_text":
        case "toolbar_field_highlight":
        case "toolbar_field_highlight_text":
          styles[color] = cssColor;
          break;
        default:
          if (
            this.experiment &&
            this.experiment.colors &&
            color in this.experiment.colors
          ) {
            styles.experimental.colors[color] = cssColor;
          } else {
            const { logger } = this.extension;
            logger.warn(`Unrecognized theme property found: colors.${color}`);
          }
          break;
      }
    }
  }

  /**
   * Helper method for loading images found in the extension's manifest.
   *
   * @param {Object} images Dictionary mapping image properties to values.
   * @param {Object} styles Styles object in which to store the colors.
   */
  loadImages(images, styles) {
    const { baseURI, logger } = this.extension;

    for (let image of Object.keys(images)) {
      let val = images[image];

      if (!val) {
        continue;
      }

      switch (image) {
        case "additional_backgrounds": {
          let backgroundImages = val.map(img => baseURI.resolve(img));
          styles.additionalBackgrounds = backgroundImages;
          break;
        }
        case "theme_frame": {
          let resolvedURL = baseURI.resolve(val);
          styles.headerURL = resolvedURL;
          break;
        }
        default: {
          if (
            this.experiment &&
            this.experiment.images &&
            image in this.experiment.images
          ) {
            styles.experimental.images[image] = baseURI.resolve(val);
          } else {
            logger.warn(`Unrecognized theme property found: images.${image}`);
          }
          break;
        }
      }
    }
  }

  /**
   * Helper method for preparing properties found in the extension's manifest.
   * Properties are commonly used to specify more advanced behavior of colors,
   * images or icons.
   *
   * @param {Object} properties Dictionary mapping properties to values.
   * @param {Object} styles Styles object in which to store the colors.
   */
  loadProperties(properties, styles) {
    let additionalBackgroundsCount =
      (styles.additionalBackgrounds && styles.additionalBackgrounds.length) ||
      0;
    const assertValidAdditionalBackgrounds = (property, valueCount) => {
      const { logger } = this.extension;
      if (!additionalBackgroundsCount) {
        logger.warn(
          `The '${property}' property takes effect only when one ` +
            `or more additional background images are specified using the 'additional_backgrounds' property.`
        );
        return false;
      }
      if (additionalBackgroundsCount !== valueCount) {
        logger.warn(
          `The amount of values specified for '${property}' ` +
            `(${valueCount}) is not equal to the amount of additional background ` +
            `images (${additionalBackgroundsCount}), which may lead to unexpected results.`
        );
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

          styles.backgroundsAlignment = val.join(",");
          break;
        }
        case "additional_backgrounds_tiling": {
          if (!assertValidAdditionalBackgrounds(property, val.length)) {
            break;
          }

          let tiling = [];
          for (let i = 0, l = styles.additionalBackgrounds.length; i < l; ++i) {
            tiling.push(val[i] || "no-repeat");
          }
          styles.backgroundsTiling = tiling.join(",");
          break;
        }
        default: {
          if (
            this.experiment &&
            this.experiment.properties &&
            property in this.experiment.properties
          ) {
            styles.experimental.properties[property] = val;
          } else {
            const { logger } = this.extension;
            logger.warn(
              `Unrecognized theme property found: properties.${property}`
            );
          }
          break;
        }
      }
    }
  }

  /**
   * Helper method for loading extension metadata required by downstream
   * consumers.
   *
   * @param {Object} extension Extension object.
   * @param {Object} styles Styles object in which to store the colors.
   */
  loadMetadata(extension, styles) {
    styles.id = extension.id;
    styles.version = extension.version;
  }

  static unload(windowId) {
    let lwtData = {
      theme: null,
    };

    if (windowId) {
      lwtData.window = getWinUtils(
        windowTracker.getWindow(windowId)
      ).outerWindowID;
      windowOverrides.delete(windowId);
    } else {
      windowOverrides.clear();
      defaultTheme = emptyTheme;
      LightweightThemeManager.fallbackThemeData = null;
    }
    onUpdatedEmitter.emit("theme-updated", {}, windowId);

    Services.obs.notifyObservers(lwtData, "lightweight-theme-styling-update");
  }
}

this.theme = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let { extension } = this;
    let { manifest } = extension;

    defaultTheme = new Theme({
      extension,
      details: manifest.theme,
      darkDetails: manifest.dark_theme,
      experiment: manifest.theme_experiment,
      startupData: extension.startupData,
    });
  }

  onShutdown(isAppShutdown) {
    if (isAppShutdown) {
      return;
    }

    let { extension } = this;
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
    let { extension } = context;

    return {
      theme: {
        getCurrent: windowId => {
          // Take last focused window when no ID is supplied.
          if (!windowId) {
            windowId = windowTracker.getId(windowTracker.topWindow);
          }
          // Force access validation for incognito mode by getting the window.
          if (!windowTracker.getWindow(windowId, context)) {
            return Promise.reject(`Invalid window ID: ${windowId}`);
          }

          if (windowOverrides.has(windowId)) {
            return Promise.resolve(windowOverrides.get(windowId).details);
          }
          return Promise.resolve(defaultTheme.details);
        },
        update: (windowId, details) => {
          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }
          }

          new Theme({
            extension,
            details,
            windowId,
            experiment: this.extension.manifest.theme_experiment,
          });
        },
        reset: windowId => {
          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }

            let theme = windowOverrides.get(windowId) || defaultTheme;
            if (theme.extension !== extension) {
              return;
            }
          } else if (defaultTheme.extension !== extension) {
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
                // Force access validation for incognito mode by getting the window.
                if (windowTracker.getWindow(windowId, context, false)) {
                  fire.async({ theme, windowId });
                }
              } else {
                fire.async({ theme });
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
