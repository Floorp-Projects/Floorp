"use strict";

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");

XPCOMUtils.defineLazyGetter(this, "gThemesEnabled", () => {
  return Preferences.get("extensions.webextensions.themes.enabled");
});

const ICONS = Preferences.get("extensions.webextensions.themes.icons.buttons", "").split(",");

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
    // A dictionary of light weight theme styles.
    this.lwtStyles = {
      icons: {},
    };
    this.baseURI = baseURI;
    this.logger = logger;
  }

  /**
   * Loads a theme by reading the properties from the extension's manifest.
   * This method will override any currently applied theme.
   *
   * @param {Object} details Theme part of the manifest. Supported
   *   properties can be found in the schema under ThemeType.
   */
  load(details) {
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
    if (!Preferences.get("extensions.webextensions.themes.icons.enabled")) {
      // Return early if icons are disabled.
      return;
    }

    for (let icon of Object.getOwnPropertyNames(icons)) {
      let val = icons[icon];
      if (!val || !ICONS.includes(icon)) {
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
   */
  unload() {
    let lwtStyles = {
      headerURL: "",
      accentcolor: "",
      additionalBackgrounds: "",
      backgroundsAlignment: "",
      backgroundsTiling: "",
      textcolor: "",
      icons: {},
    };

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

    this.theme = new Theme(extension.baseURI, extension.logger);
    this.theme.load(manifest.theme);
  }

  onShutdown() {
    if (this.theme) {
      this.theme.unload();
    }
  }

  getAPI(context) {
    let {extension} = context;

    return {
      theme: {
        update: (details) => {
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

          this.theme.load(details);
        },
      },
    };
  }
};
