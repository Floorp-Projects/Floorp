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


var comparisonView = {
  __proto__: baseView,
  setUpHandlers: function() {
    _t = this;
    //this.omv = function(event){_t.onMouseOver.apply(_t, [event]);};
    //this.omo = function(event){_t.onMouseOut.apply(_t, [event]);};
    //this.content.addEventListener("mouseover", this.omv, true);
    //this.content.addEventListener("mouseout", this.omv, true);
  },
  destroyHandlers: function() {
    //this.content.removeEventListener("mouseover", this.omv, true);
    //this.content.removeEventListener("mouseout", this.omv, true);
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
          keys.sort();
          new YAHOO.widget.HTMLNode("<pre>" + keys.join("\n") + "</pre>", fn, true, false);
        }
      }
    }
    return cat;
  }
};
var comparisonController = {
  __proto__: baseController,
  get path() {
    return 'results/' + this.tag + '/cmp-data.json';
  },
  beforeSelect: function() {
    this.result = {};
    this.isShown = false;
    comparisonView.setUpHandlers();
    var _t = this;
    var callback = function(obj) {
      delete _t.req;
      if (view != comparisonView) {
        // ignore, we have switched again
        return;
      }
      _t.result = obj;
      comparisonView.updateView(keys(_t.result));
    };
    this.req = JSON.get('results/' + this.tag + '/cmp-data.json', callback);
  },
  beforeUnSelect: function() {
    if (this.req) {
      this.req.abort();
      delete this.req;
    }
    comparisonView.destroyHandlers();
  },
  showView: function(aClosure) {
    // json onload handler does this;
  },
  getContent: function(aLoc) {
    var row = view.getCell();
    var inner = document.createElement('div');
    row.appendChild(inner);
    var r = this.result[aLoc];
    var id = "tree-" + aLoc;
    inner.id = id;
    var hasDetails = true;
    if (r.missing.total + r.missingFiles.total) {
      inner.className = "has_missing";
    }
    else if (r.obsolete.total + r.obsoleteFiles.total) {
      inner.className = "has_obsolete";
    }
    else {
      inner.className = "is_good";
      hasDetails = false;
    }
    t = new YAHOO.widget.TreeView(inner);
    var _l = aLoc;
    var rowCollector = {
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
    for each (app in kModules) {
      var class;
      if (r.tested.indexOf(app) < 0) {
        class = 'void';
      }
      else if (r.missing[app] + r.missingFiles[app]) {
        class = 'missing';
      }
      else if (r.obsolete[app] + r.obsoleteFiles[app]) {
        class = 'obsolete';
      }
      else {
        class = "good";
      }
      rowCollector.pushCell(app, 'app-res ' + class);
    }
      
    rowCollector.pushCell(r.missingFiles.total, "missing count");
    rowCollector.pushCell(r.missing.total, "missing count");
    rowCollector.pushCell(r.obsoleteFiles.total, "obsolete count");
    rowCollector.pushCell(r.obsolete.total, "obsolete count");
    rowCollector.pushCell(r.unchanged +r.changed, "good count");
    var tld = new YAHOO.widget.HTMLNode(rowCollector.content, t.getRoot(), false, hasDetails);
    if (hasDetails) {
      tld.__cl_locale = aLoc;
      tld.__do_load = true;
      tmp = new YAHOO.widget.HTMLNode("<span class='ygtvloading'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>&nbsp;loading&hellip;", tld, true, true);
      var l = aLoc;
      var _c = this;
      var expander = function(node) {
        if (!node.__do_load)
          return true;
        var callback = function(aRes) {
          comparisonView.createFullTree(aRes, node, 'missing');
          comparisonView.createFullTree(aRes, node, 'obsolete');
          delete node.__do_load;
          node.tree.removeNode(node.children[0]);
          node.refresh();
        };
        JSON.get('results/' + _c.tag + '/cmp-details-'+aLoc+'.json', callback);
      }
      t.onExpand = expander;
      delete expander;
    }
    t.draw();
    var rowToggle = function(){
      YAHOO.util.Event.addListener(tld.getContentEl(), "click",
                                   function(){tld.toggle();}, tld, true);
    };
    YAHOO.util.Event.onAvailable(tld.contentElId, rowToggle, tld);
    //YAHOO.util.Event.addListener(tld.getContentEl(), "click", rowToggle, tld, true);
    return row;
  }
};
controller.addPane('Compare', 'comparison', comparisonController, 
                   comparisonView);
//controller.addLocales(keys(results.locales));
