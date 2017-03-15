"use strict";

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");

// WeakMap[Extension -> Theme]
let themeMap = new WeakMap();

const ICONS = Preferences.get("extensions.webextensions.themes.icons.buttons", "").split(",");

/** Class representing a theme. */
class Theme {
  /**
   * Creates a theme instance.
   *
   * @param {string} baseURI The base URI of the extension, used to
   *   resolve relative filepaths.
   */
  constructor(baseURI) {
    // A dictionary of light weight theme styles.
    this.lwtStyles = {
      icons: {},
    };
    this.baseURI = baseURI;
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

    // Lightweight themes require all properties to be defined.
    if (this.lwtStyles.headerURL &&
        this.lwtStyles.accentcolor &&
        this.lwtStyles.textcolor) {
      LightweightThemeManager.fallbackThemeData = this.lwtStyles;
      Services.obs.notifyObservers(null,
        "lightweight-theme-styling-update",
        JSON.stringify(this.lwtStyles));
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
        case "headerURL":
        case "theme_frame": {
          let resolvedURL = this.baseURI.resolve(val);
          this.lwtStyles.headerURL = resolvedURL;
          break;
        }
      }
    }
  }

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
   * Unloads the currently applied theme.
   */
  unload() {
    let lwtStyles = {
      headerURL: "",
      accentcolor: "",
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

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_theme", (type, directive, extension, manifest) => {
  if (!Preferences.get("extensions.webextensions.themes.enabled")) {
    // Return early if themes are disabled.
    return;
  }

  let theme = new Theme(extension.baseURI);
  theme.load(manifest.theme);
  themeMap.set(extension, theme);
});

extensions.on("shutdown", (type, extension) => {
  let theme = themeMap.get(extension);

  // We won't have a theme if theme's aren't enabled.
  if (!theme) {
    return;
  }

  theme.unload();
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("theme", "addon_parent", context => {
  let {extension} = context;
  return {
    theme: {
      update(details) {
        let theme = themeMap.get(extension);

        // We won't have a theme if theme's aren't enabled.
        if (!theme) {
          return;
        }

        theme.load(details);
      },
    },
  };
});
