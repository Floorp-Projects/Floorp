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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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

var controller, view;

const Tier1 = ['en-GB', 'fr', 'de', 'ja', 'ja-JP-mac', 'pl', 'es-ES'];
const Tier2 = ['zh-CN', 'zh-TW', 'cs', 'da', 'nl', 'fi', 'hu', 'it', 'ko', 'pt-BR', 'pt-PT', 'ru', 'es-AR', 'sv-SE', 'tr'];
tierMap = {};
var loc;
for each (loc in Tier1) tierMap[loc] = 'Tier-1';
for each (loc in Tier2) tierMap[loc] = 'Tier-2';

view = {
  updateView: function() {
    this._gView = document.getElementById("view");
    var cmp = function(l,r) {
      var lName = results.details[l].ShortName;
      var rName =  results.details[r].ShortName;
      if (lName) lName = lName.toLowerCase();
      if (rName) rName = rName.toLowerCase();
      return lName < rName ? -1 : lName > rName ? 1 : 0;
    }
    for (loc in results.locales) {
      var row = document.createElement("tr");
      if (tierMap[loc])
        row.className += ' ' + tierMap[loc];
      else
        row.className += ' Tier-3';
      this._gView.appendChild(row);
      var lst = results.locales[loc].list;
      lst.sort(cmp);
      var orders = results.locales[loc].orders;
      if (orders) {
        var explicit = [];
        var implicit = [];
        for (var sn in orders) {
          if (explicit[sn]) {
            fatal = true;
            break;
          }
          explicit[sn] = orders[sn];
        }
        explicit = [];
        for each (var path in lst) {
          var shortName = results.details[path].ShortName;
          if (orders[shortName]) {
            explicit[orders[shortName] - 1] = path;
          }
          else {
            implicit.push(path);
          }
        }
        lst = explicit.concat(implicit);
      }
      var content = '';
      for each (var path in lst) {
        YAHOO.widget.Logger.log('testing ' + path);
        var shortName = results.details[path].ShortName;
        var cl = '';
        if (results.locales[loc].orders && results.locales[loc].orders[shortName]) {
          cl = " ordered";
        }
        content += '<td class="searchplugin' + cl + '">' + shortName + '</td>';
      }
      row.innerHTML = '<td class="locale">' + loc + '</td>' + content;
    }
  },
  onClickDisplay: function(event) {
    YAHOO.util.Event.stopEvent(event);
    if (event.target.localName != 'INPUT') {
      return false;
    }
    var handler = function() {return function(){view.onCDReal(event.target)}};
    window.setTimeout(handler(),0);
    return false;
  },
  onCDReal: function(target) {
    var dsp = !target.checked;
    target.checked = dsp;
    dsp = dsp ? 'block' : 'none';
    var layout = document.styleSheets[2];
    layout.insertRule('div.' + target.name + ' {display: ' + dsp + ';}',
                      layout.cssRules.length);
    return false;
  }
};

controller = {
  kPrefix: 'results/',
}

function onLoad(event) {
  document.removeEventListener("load", onLoad, true);
  document.getElementById("checks").addEventListener("click", view.onClickDisplay, true);
  view.updateView();
  return true;
}
