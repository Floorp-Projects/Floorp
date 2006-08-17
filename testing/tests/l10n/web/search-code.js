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

var view;
view = {
  hashes: {},
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
      row.innerHTML = '<td class="locale"><a href="https://bugzilla.mozilla.org/buglist.cgi?query_format=advanced&amp;short_desc_type=regexp&amp;short_desc=' + loc + '%5B%20-%5D&amp;chfieldto=Now&amp;field0-0-0=blocked&amp;type0-0-0=substring&amp;value0-0-0=347914">' + loc + '</a></td>';
      for each (var path in lst) {
        //YAHOO.widget.Logger.log('testing ' + path);
        var innerContent;
        var cl = '';
        if (path.match(/^mozilla/)) {
          cl += ' enUS';
        }
        var localName = path.substr(path.lastIndexOf('/') + 1)
        if (results.details[path].error) {
          innerContent = 'error in ' + localName;
          cl += ' error';
        }
        else {
          var shortName = results.details[path].ShortName;
          var img = results.details[path].Image;
          if (results.locales[loc].orders && results.locales[loc].orders[shortName]) {
            cl += " ordered";
          }
          innerContent = '<img src="' + img + '">' + shortName;
        }
        var td = document.createElement('td');
        td.className = 'searchplugin' + cl;
        td.innerHTML = innerContent;
        row.appendChild(td);
        td.details = results.details[path];
        // test the hash code
        if (td.details.error) {
          // ignore errorenous plugins
          continue;
        }
        if (this.hashes[localName]) {
          this.hashes[localName].nodes.push(td);
          if (this.hashes[localName].conflict) {
            td.className += ' conflict';
          }
          else if (this.hashes[localName].key != td.details.md5) {
            this.hashes[localName].conflict = true;
            for each (td in this.hashes[localName].nodes) {
              td.className += ' conflict';
            }
          }
        }
        else {
          this.hashes[localName] =  {key: td.details.md5, nodes: [td]};
        }
      }
    }
    for (localName in this.hashes) {
      if ( ! ('conflict' in this.hashes[localName])) {
        continue;
      }
      locs = [];
      for each (var td in this.hashes[localName].nodes) {
        locs.push(td.parentNode.firstChild.textContent);
      }
      YAHOO.widget.Logger.log('difference in ' + localName + ' for ' + locs.join(', '));
    }
  },
  onMouseOver: function(event) {
    if (!event.target.details)
      return;
    var _e = {
      details: event.target.details,
      clientX: event.clientX,
      clientY: event.clientY
    };
    view.pending = setTimeout(function(){view.showDetail(_e);}, 500);
  },
  onMouseOut: function(event) {
    if (view.pending) {
      clearTimeout(view.pending);
      delete view.pending;
      return;
    }
    if (!event.target.details)
      return;
  },
  showDetail: function(event) {
    delete view.pending;
    if (!view._dv) {
      view._dv = new YAHOO.widget.Panel('dv', {visible:true,draggable:true,constraintoviewport:true});
      view._dv.beforeHideEvent.subscribe(function(){delete view._dv;}, null);
    }
    var dt = event.details;
    if (dt.error) {
      view._dv.setHeader("Error");
      view._dv.setBody(dt.error);
    }
    else {
      view._dv.setHeader("Details");
      var c = '';
      var q = 'test';
      var len = 0;
      for each (var url in dt.urls) {
        var uc = url.template;
        var sep = (uc.indexOf('?') > -1) ? '&amp;' : '?';
        for (var p in url.params) {
          uc += sep + p + '=' + url.params[p];
          sep = '&amp;';
        }
        uc = uc.replace('{searchTerms}', q);
        c += '<button onclick="window.open(this.textContent)">' + uc + '</button><br>';
        len  = len < c.length ? c.length : len;
      }
      view._dv.setBody(c);
    }
    view._dv.render(document.body);
    view._dv.moveTo(event.clientX + window.scrollX, event.clientY + window.scrollY);
  },
  onClickDisplay: function(event) {
    if (event.target.localName != 'INPUT') {
      return false;
    }
    YAHOO.widget.Logger.log('clicking ' + event);
    var handler = function() {return function(){view.onCDReal(event.target)}};
    window.setTimeout(handler(),0);
    return false;
  },
  onCDReal: function(target) {
    var dsp = target.checked;
    target.checked = dsp;
    dsp = dsp ? 'table-row' : 'none';
    var layout = document.styleSheets[document.styleSheets.length - 1];
    var i = Number(target.name);
    layout.deleteRule(i);
    layout.insertRule('.Tier-' + (i+1) + ' {display: ' + dsp + ';}', i);
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
  document.getElementById("view").addEventListener("mouseover", view.onMouseOver, true);
  document.getElementById("view").addEventListener("mouseout", view.onMouseOut, true);
  return true;
}
