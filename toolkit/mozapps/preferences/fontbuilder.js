# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var FontBuilder = {
  _enumerator: null,
  get enumerator ()
  {
    if (!this._enumerator) {
      this._enumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"]
                                   .createInstance(Components.interfaces.nsIFontEnumerator);
    }
    return this._enumerator;
  },

  _allFonts: null,
  buildFontList: function (aLanguage, aFontType, aMenuList) 
  {
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
        }
        else {
          builtItem = builtItem.nextSibling;
          builtItemValue = builtItem ? builtItem.getAttribute("value") : null;
        }
      }
    }
    aMenuList.appendChild(popup);    
  }
};
