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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Dao Gottwald <dao@mozilla.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

let EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._footerId = aDocument.documentElement.getAttribute("lightweightthemesfooter");

  Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService)
            .addObserver(this, "lightweight-theme-styling-update", false);

  var temp = {};
  Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
  this._update(temp.LightweightThemeManager.currentThemeForDisplay);
}

LightweightThemeConsumer.prototype = {
  observe: function (aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    this._update(JSON.parse(aData));
  },

  destroy: function () {
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .removeObserver(this, "lightweight-theme-styling-update");

    this._doc = null;
  },

  _update: function (aData) {
    if (!aData)
      aData = { headerURL: "", footerURL: "", textcolor: "", accentcolor: "" };

    var root = this._doc.documentElement;
    var active = !!aData.headerURL;

    if (active) {
      root.style.color = aData.textcolor || "black";
      root.style.backgroundColor = aData.accentcolor || "white";
      let [r, g, b] = _parseRGB(this._doc.defaultView.getComputedStyle(root, "").color);
      let luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
      root.setAttribute("lwthemetextcolor", luminance <= 110 ? "dark" : "bright");
      root.setAttribute("lwtheme", "true");
    } else {
      root.style.color = "";
      root.style.backgroundColor = "";
      root.removeAttribute("lwthemetextcolor");
      root.removeAttribute("lwtheme");
    }

    _setImage(root, active, aData.headerURL);
    if (this._footerId) {
      let footer = this._doc.getElementById(this._footerId);
      footer.style.backgroundColor = active ? aData.accentcolor || "white" : "";
      _setImage(footer, active, aData.footerURL);
      if (active && aData.footerURL)
        footer.setAttribute("lwthemefooter", "true");
      else
        footer.removeAttribute("lwthemefooter");
    }

#ifdef XP_MACOSX

    if (active && aData.accentcolor) {
      root.setAttribute("activetitlebarcolor", aData.accentcolor);
      root.setAttribute("inactivetitlebarcolor", aData.accentcolor);
    } else {
      root.removeAttribute("activetitlebarcolor");
      root.removeAttribute("inactivetitlebarcolor");
    }

    if (active)
      root.setAttribute("drawintitlebar", "true");
    else
      root.removeAttribute("drawintitlebar");
#endif
  }
}

function _setImage(aElement, aActive, aURL) {
  aElement.style.backgroundImage =
    (aActive && aURL) ? 'url("' + aURL.replace('"', '\\"', "g") + '")' : "";
}

function _parseRGB(aColorString) {
  var rgb = aColorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
  rgb.shift();
  return rgb.map(function (x) parseInt(x));
}
