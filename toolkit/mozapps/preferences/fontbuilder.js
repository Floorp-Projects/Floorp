// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../content/preferencesBindings.js */

var FontBuilder = {
  _enumerator: null,
  get enumerator() {
    if (!this._enumerator) {
      this._enumerator = Cc["@mozilla.org/gfx/fontenumerator;1"]
                           .createInstance(Ci.nsIFontEnumerator);
    }
    return this._enumerator;
  },

  _allFonts: null,
  _langGroupSupported: false,
  async buildFontList(aLanguage, aFontType, aMenuList) {
    // Reset the list
    while (aMenuList.hasChildNodes())
      aMenuList.firstChild.remove();

    let defaultFont = null;
    // Load Font Lists
    let fonts = await this.enumerator.EnumerateFontsAsync(aLanguage, aFontType);
    if (fonts.length > 0)
      defaultFont = this.enumerator.getDefaultFont(aLanguage, aFontType);
    else {
      fonts = await this.enumerator.EnumerateFontsAsync(aLanguage, "");
      if (fonts.length > 0)
        defaultFont = this.enumerator.getDefaultFont(aLanguage, "");
    }

    if (!this._allFonts)
      this._allFonts = await this.enumerator.EnumerateAllFontsAsync({});

    // Build the UI for the Default Font and Fonts for this CSS type.
    const popup = document.createElement("menupopup");
    let separator;
    if (fonts.length > 0) {
      let menuitem = document.createElement("menuitem");
      if (defaultFont) {
        document.l10n.setAttributes(menuitem, "fonts-label-default", {
          name: defaultFont
        });
      } else {
        document.l10n.setAttributes(menuitem, "fonts-label-default-unnamed");
      }
      menuitem.setAttribute("value", ""); // Default Font has a blank value
      popup.appendChild(menuitem);

      separator = document.createElement("menuseparator");
      popup.appendChild(separator);

      for (let font of fonts) {
        menuitem = document.createElement("menuitem");
        menuitem.setAttribute("value", font);
        menuitem.setAttribute("label", font);
        popup.appendChild(menuitem);
      }
    }

    // Build the UI for the remaining fonts.
    if (this._allFonts.length > fonts.length) {
      this._langGroupSupported = true;
      // Both lists are sorted, and the Fonts-By-Type list is a subset of the
      // All-Fonts list, so walk both lists side-by-side, skipping values we've
      // already created menu items for.
      let builtItem = separator ? separator.nextSibling : popup.firstChild;
      let builtItemValue = builtItem ? builtItem.getAttribute("value") : null;

      separator = document.createElement("menuseparator");
      popup.appendChild(separator);

      for (let font of this._allFonts) {
        if (font != builtItemValue) {
          const menuitem = document.createElement("menuitem");
          menuitem.setAttribute("value", font);
          menuitem.setAttribute("label", font);
          popup.appendChild(menuitem);
        } else {
          builtItem = builtItem.nextSibling;
          builtItemValue = builtItem ? builtItem.getAttribute("value") : null;
        }
      }
    }
    aMenuList.appendChild(popup);
  },

  readFontSelection(aElement) {
    // Determine the appropriate value to select, for the following cases:
    // - there is no setting
    // - the font selected by the user is no longer present (e.g. deleted from
    //   fonts folder)
    const preference = Preferences.get(aElement.getAttribute("preference"));
    if (preference.value) {
      const fontItems = aElement.getElementsByAttribute("value", preference.value);

      // There is a setting that actually is in the list. Respect it.
      if (fontItems.length)
        return undefined;
    }

    // Otherwise, use "default" font of current system which is computed
    // with "font.name-list.*".  If "font.name.*" is empty string, it means
    // "default".  So, return empty string in this case.
    return "";
  }
};
