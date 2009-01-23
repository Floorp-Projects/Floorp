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


var waterfallView = {
  __proto__: baseView,
  scale: 1/60,
  unstyle: function() {
    this.content.innerHTML = '';
  },
  updateView: function(aResult) {
    YAHOO.widget.Logger.log('waterfallView.updateView called');
    var head = document.getElementById('head');
    head.innerHTML = '<th></th>';
    this.content.innerHTML = '';
    for each (var loc in controller.locales) {
      var h = document.createElement('th');
      h.className = 'wf-locale-head ' + view.getClass(loc);
      h.textContent = loc;
      head.appendChild(h);
    }
    var heads = head.childNodes;
    var lastDate = -1;
    function pad(aNumber, length) {
      str = String(aNumber);
      while (str.length < length) {
        str = '0' + str;
      }
      return str;
    };
    for (var i = aResult.length - 1; i >= 0; i--) {
      var wf = document.createElement('tr');
      wf.className = 'waterfall-row';
      wf.setAttribute('style','vertical-align: top;');
      this.content.appendChild(wf);
      var b = aResult[i];
      var height = Math.floor((b[1][1] - b[1][0]) * this.scale);
      var style = "height: " + height + "px;";
      wf.setAttribute('style', style);
      var cell = view.getCell();
      cell.className = 'cell-label';
      var d = this.getUTCDate(b[0]);
      var label = pad(d.getUTCHours(), 2) + ':' +
        pad(d.getUTCMinutes(), 2);
      if (lastDate != d.getUTCDate()) {
        lastDate = d.getUTCDate();
        label = d.getUTCDate() +'/'+ (d.getUTCMonth()+1) + '<br>' + label;
      }
      cell.innerHTML = label +
        '<br><a href="javascript:controller.showLog(\'' + b[0] + '\');">L</a>';
      wf.appendChild(cell);
      var locs = keys(b[2]);
      var h = 1;
      for each (loc in locs) {
        if (h >= heads.length) {
          YAHOO.widget.Logger.log("dropping result for " + loc);
          continue;
        }        
        while (heads[h].textContent < loc) {
          // we don't have a result for this column in this build
          cell = view.getCell();
          cell.className = view.getClass(heads[h].textContent)
          wf.appendChild(cell);
          h++;
          if (h >= heads.length) {
            YAHOO.widget.Logger.log("dropping result for " + loc);
            continue;
          }
        }
        if (heads[h].textContent > loc) {
          YAHOO.widget.Logger.log("dropping result for " + loc);
          continue;
        }
        cell = view.getCell();
        cell.innerHTML = '<a href="javascript:controller.showLog(\'' + 
          b[0] + '\',\'' + loc + '\');">L</a> <a href="javascript:controller.showDetails(\'' + 
          b[0] + '\',\'' + loc + '\');">B</a>';
        if (b[2][loc] & 2) {
          cell.className = 'busted ';
        }
        else if (b[2][loc] & 1) {
          cell.className = 'fair ';
        }
        else {
          cell.className = 'good ';
        }
        cell.className += ' ' + view.getClass(loc);
        wf.appendChild(cell);
        h++;
      }
      if (i == 0) {
        continue;
      }
      // XXX don't make output sparse
      continue;
      wf = document.createElement('tr');
      wf.className = 'waterfall-row';
      wf.setAttribute('style','vertical-align: top;');
      this.content.appendChild(wf);
      var b = aResult[i];
      var height = Math.floor((b[1][0] - aResult[i-1][1][1]) * this.scale);
      var style = "height: " + height + "px;";
      wf.setAttribute('style', style);
      //var cell = view.getCell();
    }
  },
  setUpHandlers: function() {
    _t = this;
  },
  destroyHandlers: function() {
  },
  // Helper functions
  getUTCDate: function(aTag) {
    var D = new Date();
    var d = aTag.split(/[ -]/).map(function(aEl){return Number(aEl);});
    d[1] -= 1; // adjust month
    D.setTime(Date.UTC.apply(null,d));
    return D;
  }
};
var waterfallController = {
  __proto__: baseController,
  get path() {
    return 'results/waterfall.json';
  },
  beforeSelect: function() {
    this.result = {};
    this.isShown = false;
    waterfallView.setUpHandlers();
    var _t = this;
    var callback = function(obj) {
      delete _t.req;
      if (view != waterfallView) {
        // ignore, we have switched again
        return;
      }
      _t.result = obj;
      _t.tag = obj[obj.length - 1][0]; // set the subdir to latest build
      controller.addLocales(keys(obj[obj.length - 1][2]));
      waterfallView.updateView(_t.result);
    };
    this.req = JSON.get('results/waterfall.json', callback);
  },
  beforeUnSelect: function() {
    if (this.req) {
      this.req.abort();
      delete this.req;
    }
    waterfallView.unstyle();
    waterfallView.destroyHandlers();
  },
  showView: function(aClosure) {
    // json onload handler does this;
  }
};
controller.addPane('Tinder', 'waterfall', waterfallController, 
                   waterfallView);
