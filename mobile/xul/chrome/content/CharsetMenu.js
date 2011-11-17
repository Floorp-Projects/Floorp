/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      let charset = getBrowser().documentCharsetInfo.forcedCharset;
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
    let currentCharset = getBrowser().documentCharsetInfo.forcedCharset;
    
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
