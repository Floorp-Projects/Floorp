// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var FontBuilder = {
  _enumerator: null,
  get enumerator() {
    if (!this._enumerator) {
      this._enumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"]
                                   .createInstance(Components.interfaces.nsIFontEnumerator);
    }
    return this._enumerator;
  },

  _allFonts: null,
  _langGroupSupported: false,
  buildFontList(aLanguage, aFontType, aMenuList) {
    // Reset the list
    while (aMenuList.hasChildNodes())
      aMenuList.removeChild(aMenuList.firstChild);

    var defaultFont = null;
    // Load Font Lists
    var fonts = this.enumerator.EnumerateFonts(aLanguage, aFontType, { } );
    if (fonts.length > 0)
      defaultFont = this.enumerator.getDefaultFont(aLanguage, aFontType);
    else {
      fonts = this.enumerator.EnumerateFonts(aLanguage, "", { });
      if (fonts.length > 0)
        defaultFont = this.enumerator.getDefaultFont(aLanguage, "");
    }

    if (!this._allFonts)
      this._allFonts = this.enumerator.EnumerateAllFonts({});

    // Build the UI for the Default Font and Fonts for this CSS type.
    var popup = document.createElement("menupopup");
    var separator;
    if (fonts.length > 0) {
      if (defaultFont) {
        var bundlePreferences = document.getElementById("bundlePreferences");
        var label = bundlePreferences.getFormattedString("labelDefaultFont", [defaultFont]);
        var menuitem = document.createElement("menuitem");
        menuitem.setAttribute("label", label);
        menuitem.setAttribute("value", ""); // Default Font has a blank value
        popup.appendChild(menuitem);

        separator = document.createElement("menuseparator");
        popup.appendChild(separator);
      }

      for (var i = 0; i < fonts.length; ++i) {
        menuitem = document.createElement("menuitem");
        menuitem.setAttribute("value", fonts[i]);
        menuitem.setAttribute("label", fonts[i]);
        popup.appendChild(menuitem);
      }
    }

    // Build the UI for the remaining fonts.
    if (this._allFonts.length > fonts.length) {
      this._langGroupSupported = true;
      // Both lists are sorted, and the Fonts-By-Type list is a subset of the
      // All-Fonts list, so walk both lists side-by-side, skipping values we've
      // already created menu items for.
      var builtItem = separator ? separator.nextSibling : popup.firstChild;
      var builtItemValue = builtItem ? builtItem.getAttribute("value") : null;

      separator = document.createElement("menuseparator");
      popup.appendChild(separator);

      for (i = 0; i < this._allFonts.length; ++i) {
        if (this._allFonts[i] != builtItemValue) {
          menuitem = document.createElement("menuitem");
          menuitem.setAttribute("value", this._allFonts[i]);
          menuitem.setAttribute("label", this._allFonts[i]);
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
    let preference = document.getElementById(aElement.getAttribute("preference"));
    if (preference.value) {
      let fontItems = aElement.getElementsByAttribute("value", preference.value);

      // There is a setting that actually is in the list. Respect it.
      if (fontItems.length)
        return undefined;
    }

    // The first item will be a reasonable choice only if the font backend
    // supports language-specific enumaration.
    let defaultValue = this._langGroupSupported ?
                       aElement.firstChild.firstChild.getAttribute("value") : "";
    let fontNameList = preference.name.replace(".name.", ".name-list.");
    let prefFontNameList = document.getElementById(fontNameList);
    if (!prefFontNameList || !prefFontNameList.value)
      return defaultValue;

    let fontNames = prefFontNameList.value.split(",");

    for (let i = 0; i < fontNames.length; ++i) {
      let fontName = this.enumerator.getStandardFamilyName(fontNames[i].trim());
      let fontItems = aElement.getElementsByAttribute("value", fontName);
      if (fontItems.length)
        return fontItems[0].getAttribute("value");
    }
    return defaultValue;
  }
};
