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

const kModules = ['browser','mail','toolkit'];
const Tier1 = ['en-GB', 'fr', 'de', 'ja', 'ja-JP-mac', 'pl', 'es-ES'];
const Tier2 = ['zh-CN', 'zh-TW', 'cs', 'da', 'nl', 'fi', 'hu', 'it', 'ko', 'pt-BR', 'pt-PT', 'ru', 'es-AR', 'sv-SE', 'tr'];
tierMap = {};
for each (var loc in Tier1) tierMap[loc] = 'Tier-1';
for each (var loc in Tier2) tierMap[loc] = 'Tier-2';

var calHelper = {
  formatDate : function(aDate) {
    function format(aNum) {
      if (aNum < 10)
        return '0' + aNum;
      return String(aNum);
    }
    return aDate.getUTCFullYear() + '-' 
    + format(aDate.getUTCMonth() + 1) + '-'
    + format(aDate.getUTCDate());
  }
};

function Observer(aLoc, aNode, aDate) {
  this._loc = aLoc;
  this._node = aNode;
  this._date = aDate;
}
Observer.prototype = {
  handleSuccess: function(aData) {
    r = eval('('+aData+')');
    view.createFullTree(r, this._node, "missing");
    view.createFullTree(r, this._node, "obsolete");
    delete this._node.__do_load;
    this._node.tree.removeNode(this._node.children[0]);
    this._node.refresh();
  },
  handleFailure: function() {
    throw "couldn't load details";
  }
};


view = {
  setupResults: function(responseText) {
    res = eval('(' + responseText + ')');
    this._locales = [];
    for (var loc in res) {
      this._locales.push(loc);
    }
    this._locales.sort();
    this._results = res;
  },
  updateDateView: function(aDate) {
    if (!aDate)
      throw "Date expected in view.updateDateView";
    this._gView = document.getElementById("view");
    document.getElementById('currentDate').textContent = aDate;
    var oldIndex = 0;
    for each (loc in this._locales) {
      var r = this._results[loc];
      var id = "tree-" + loc;
      var oldChild = this._gView.childNodes[oldIndex++];
      var t;
      if (oldChild) {
        t = oldChild;
        t.innerHTML = '';
        if (t.id != id) {
          // we don't have the right div, make sure to remove the ID
          // from the pre-existing one, just in case
          var obsDiv = document.getElementById(id);
          if (obsDiv) {
            obsDiv.id = '';
          }
        }
      }
      else {
        t = document.createElement("div");
      }
      t.id = id;
      var hasDetails = true;
      if (r.missing.total + r.missingFiles.total) {
        t.className = "has_missing";
      }
      else if (r.obsolete.total + r.obsoleteFiles.total) {
        t.className = "has_obsolete";
      }
      else {
        t.className = "is_good";
        hasDetails = false;
      }
      if (tierMap[loc])
        t.className += ' ' + tierMap[loc];
      else
        t.className += ' Tier-3';
      if (!oldChild) {
        this._gView.appendChild(t);
      }
      t = new YAHOO.widget.TreeView("tree-" + loc);
      var closure = function() {
        var _loc = loc;
        var _d = aDate;
        return  function(node) {
          if (!node.__do_load)
            return true;
          var callback = new Observer(_loc, node, _d);
          controller.getDetails(callback._date, callback._loc, callback);
          delete callback;
          return true;
        };
      }
      if (hasDetails) {
        t.onExpand = closure();
      }
      var row = {
        _c : '',
        pushCell: function(content, class) {
          this._c += '<td';
          if (content == 0)
            class += ' zero'
          if (class) 
            this._c += ' class="' + class + '"';
          this._c += '>' + content + '</td>';
        },
        get content() {return '<table class="locale-row"><tr>' + 
                       this._c + '</tr></table>';}
      };
      row.pushCell(loc, "locale");
      var res = r;
      for each (app in kModules) {
        var class;
        if (res.tested.indexOf(app) < 0) {
          class = 'void';
        }
        else if (res.missing[app] + res.missingFiles[app]) {
          class = 'missing';
        }
        else if (res.obsolete[app] + res.obsoleteFiles[app]) {
          class = 'obsolete';
        }
        else {
          class = "good";
        }
        row.pushCell(app, 'app-res ' + class);
      }
      
      row.pushCell(r.missingFiles.total, "missing count");
      row.pushCell(r.missing.total, "missing count");
      row.pushCell(r.obsoleteFiles.total, "obsolete count");
      row.pushCell(r.obsolete.total, "obsolete count");
      row.pushCell(r.unchanged +r.changed, "good count");
      var tld = new YAHOO.widget.HTMLNode(row.content, t.getRoot(), false, hasDetails);
      if (hasDetails) {
        tld.__cl_locale = loc;
        tld.__cl_date = aDate;
        tld.__do_load = true;
        tmp = new YAHOO.widget.HTMLNode("<span class='ygtvloading'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>&nbsp;loading&hellip;", tld, true, true);
      }
      t.draw();
      var rowToggle = function(e){
        this.toggle()
      };
      YAHOO.util.Event.addListener(tld.getContentEl(), "click", rowToggle, tld, true);
    };
    while ((oldChild = this._gView.childNodes[oldIndex])) {
      this._gView.removeChild(oldChild);
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
  },
  // Helper methods
  createFullTree: function ct(aDetails, aNode, aName) {
    if (!aDetails || ! (aDetails instanceof Object))
      return;
    var cat = new YAHOO.widget.TextNode(aName, aNode, true);
    for each (postfix in ["Files", ""]) {
      var subRes = aDetails[aName + postfix];
      var append = postfix ? ' (files)' : '';
      for (mod in subRes) {
        var mn = new YAHOO.widget.TextNode(mod + append, cat, true);
        for (fl in subRes[mod]) {
          if (postfix) {
            new YAHOO.widget.TextNode(subRes[mod][fl], mn, false);
            continue;
          }
          var fn = new YAHOO.widget.TextNode(fl, mn, false);
          var keys = [];
          for each (k in subRes[mod][fl]) {
            keys.push(k);
          }
          new YAHOO.widget.HTMLNode("<pre>" + keys.join("\n") + "</pre>", fn, true, false);
        }
      }
    }
    return cat;
  }
};

controller = {
  _requests : [],
  getDataForCal : function() {
    var d = cal.getSelectedDates()[0];
    d.setHours(12);
    d = new Date(d.valueOf() - d.getTimezoneOffset() * 60000)
    var callback = {
      _date: calHelper.formatDate(d),
      handleSuccess: function(aData) {
        view.setupResults(aData);
        view.updateDateView(this._date);
      },
      handleFailure: function() {
        throw "couldn't load something";
      }
    };
    this.getData(callback._date, callback);
  },
  kPrefix: 'results/',
  getData : function _gda(aDate, aCallback) {
    return this._getInternal(aDate, 'data-', null, aCallback);
  },
  getDetails : function _gde(aDate, aLocale, aCallback) {
    return this._getInternal(aDate, 'details-', aLocale, aCallback);
  },
  _getInternal : function _gi(aDate, aName, aPostfix, aCallback) {
    var url = this.kPrefix + aName + aDate;
    if (aPostfix)
      url += '-' + aPostfix;
    url += '.js';
    var req = this._getRequest();
    req.overrideMimeType("text/plain"); // don't parse XML, this is js
    req.onerror = function(event) {controller.handleFailure(event.target);event.stopPropagation();};
    req.onload = function(event) {controller.handleSuccess(event.target);event.stopPropagation();};
    if (aCallback) {
      req.callback = aCallback;
    }
    req.open("GET", url, true);
    try {
      req.send(null);
    } catch (e) {
      // work around send sending errors, but not the callback
      controller.handleFailure(req);
    }
    return true;
  },
  _getRequest : function() {
    for each (var req in this._requests) {
      if (!req.status)
        return req;
    }
    req = new XMLHttpRequest();
    this._requests.push(req);
    return req;
  },
  // Ajax interface
  handleSuccess : function(o) {
    if (o.responseText === undefined)
      throw "expected response text in handleSuccess";
    if (o.callback) {
      o.callback.handleSuccess(o.responseText);
      delete o.callback;
    }
  },
  handleFailure : function(o) {
    if (o.callback) {
      o.callback.handleFailure();
      delete o.callback;
      return;
    }
    throw("load failed with " + o.status + " " + o.statusText);
  }
}

var cal;
function onLoad(event) {
  document.removeEventListener("load", onLoad, true);
  document.getElementById("checks").addEventListener("click", view.onClickDisplay, true); 
  cal = new YAHOO.widget.Calendar('cal', 'calDiv');
  cal.minDate = new Date(2006, 5, 1);
  //  cal.maxDate = new Date();
  cal.render();
  cal.onSelect = function() {controller.getDataForCal();};

  var n = new Date();
  if (n.getUTCHours() < 12  && n.getUTCDate() == n.getDate()) {
    // do yesterday, we didn't check out today yet
    n = new Date(n - YAHOO.widget.DateMath.ONE_DAY_MS);
  }
  n = new Date(n.getFullYear(), n.getMonth(), n.getDate());
  cal.select(n);

  return true;
}
