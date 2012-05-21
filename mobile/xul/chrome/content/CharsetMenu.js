/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var CharsetMenu = {
  _strings: null,
  _charsets: null,

  get strings() {
    if (!this._strings)
      this._strings = Services.strings.createBundle("chrome://global/locale/charsetTitles.properties");
    return this._strings;
  },

  init: function() {
    PageActions.register("pageaction-charset", this.updatePageAction, this);
  },

  updatePageAction: function(aNode) {
    let pref = Services.prefs.getComplexValue("browser.menu.showCharacterEncoding", Ci.nsIPrefLocalizedString).data;
    if (pref == "true") {
      let charset = getBrowser().docShell.forcedCharset;
      if (charset) {
        charset = charset.toString();
        charset = charset.trim().toLowerCase();
        aNode.setAttribute("description", this.strings.GetStringFromName(charset + ".title"));
      } else if (aNode.hasAttribute("description")) {
        aNode.removeAttribute("description");
      }
    }
    return ("true" == pref)
  },

  _toMenuItems: function(aCharsets, aCurrent) {
    let ret = [];
    aCharsets.forEach(function (aSet) {
      try {
        let string = aSet.trim().toLowerCase();
        ret.push({
          label: this.strings.GetStringFromName(string + ".title"),
          value: string,
          selected: (string == aCurrent)
        });
      } catch(ex) { }
    }, this);
    return ret;
  },

  menu : {
    dispatchEvent: function(aEvent) {
      if (aEvent.type == "command")
        CharsetMenu.setCharset(this.menupopup.children[this.selectedIndex].value);
    },
    menupopup: {
      hasAttribute: function(aAttr) { return false; },
    },
    selectedIndex: -1
  },

  get charsets() {
    if (!this._charsets) {
      this._charsets = Services.prefs.getComplexValue("intl.charsetmenu.browser.static", Ci.nsIPrefLocalizedString).data.split(",");
    }
    let charsets = this._charsets;
    let currentCharset = getBrowser().docShell.forcedCharset;
    
    if (currentCharset) {
      currentCharset = currentCharset.toString();
      currentCharset = currentCharset.trim().toLowerCase();
      if (charsets.indexOf(currentCharset) == -1)
        charsets.splice(0, 0, currentCharset);
    }
    return this._toMenuItems(charsets, currentCharset);
  },

  show: function showCharsetMenu() {
    this.menu.menupopup.children = this.charsets;
    MenuListHelperUI.show(this.menu);
  },

  setCharset: function setCharset(aCharset) {
    let browser = getBrowser();
    browser.messageManager.sendAsyncMessage("Browser:SetCharset", {
      charset: aCharset
    });
    let history = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    history.setCharsetForURI(browser.documentURI, aCharset);
  }
};
