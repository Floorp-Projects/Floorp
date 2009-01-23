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
 * The Original Code is mozilla.org l10n testing.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2066
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <l10n@mozilla.com>
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

/**
 * Item to close a menu popup
 *
 * Used by OpenMenuObj.
 */
function CloseMenuObj(aPopup) {
  this.args = [aPopup];
}
CloseMenuObj.prototype = {
  args: null,
  method: function _openMenu(aPopup) {
    aPopup.hidePopup();
  }
};

/**
 * Item to open a menu popup
 *
 * Adds a CloseMenuObj item as well as potentially child popups to
 * Stack
 */
function OpenMenuObj(aPopup) {
  this.args = [aPopup];
}
OpenMenuObj.prototype = {
  args: null,
  method: function _openMenu(aPopup) {
    aPopup.showPopup();
    Stack.push(new CloseMenuObj(aPopup));
    for (var i = aPopup.childNodes.length - 1; i>=0; --i) {
      var c = aPopup.childNodes[i];
      if (c.nodeName != 'menu' || c.childNodes.length == 0) {
        continue;
      }
      for each (var childpop in c.childNodes) {
        if (childpop.localName == "menupopup") {
          Stack.push(new OpenMenuObj(childpop));
        }
      }
    }
  }
};

/**
 * Item to kick off menu coverage testing
 *
 * Pretty similar to OpenMenuObj, just that it works on the main-menubar
 * instead of a given popup.
 */
function RootMenu(aWindow) {
  this._w = aWindow;
};
RootMenu.prototype = {
  args: [],
  method: function() {
    var mb = this._w.document.getElementById('main-menubar');
    for (var i = mb.childNodes.length - 1; i>=0; --i) {
      var m = mb.childNodes[i];
      Stack.push(new OpenMenuObj(m.firstChild));
    }
  },
  _w: null
};

toRun.push(new TestDone('MENUS'));
toRun.push(new RootMenu(wins[0]));
toRun.push(new TestStart('MENUS'));
