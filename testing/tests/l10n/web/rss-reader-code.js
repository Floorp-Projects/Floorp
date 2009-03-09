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


var rssController = {
  __proto__: baseController,
  get path() {
    return 'results/' + this.tag + '/feed-reader-results.json';
  },
  beforeSelect: function() {
    this.hashes = {};
    rssView.setUpHandlers();
    var _t = this;
    var callback = function(obj) {
      delete _t.req;
      if (view != rssView) {
        // ignore, we have switched again
        return;
      }
      _t.result = obj;
      rssView.updateView(keys(_t.result));
    };
    this.req = JSON.get('results/' + this.tag + '/feed-reader-results.json',
                        callback);
  },
  beforeUnSelect: function() {
    rssView.destroyHandlers();
  },
  showView: function(aClosure) {
    // json onload handler does this;
  },
  getContent: function(aLoc) {
    var row = document.createDocumentFragment();
    var lst = this.result[aLoc];
    var _t = this;
    for each (var pair in lst) {
      //YAHOO.widget.Logger.log('testing ' + path);
      var td = document.createElement('td');
      td.className = 'feedreader';
      td.innerHTML = pair[0];
      td.title = pair[1];
      row.appendChild(td);
    }
    return row;
  }
};
var rssView = {
  __proto__: baseView,
  setUpHandlers: function() {
    _t = this;
  },
  destroyHandlers: function() {
    if (this._dv) {
      this._dv.hide();
    }
  }
};
controller.addPane('RSS', 'rss', rssController, rssView);
//controller.addLocales(keys(results.locales));
