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


var bookmarksView = {
  __proto__: baseView,
  setUpHandlers: function() {
    _t = this;
  },
  destroyHandlers: function() {
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
var bookmarksController = {
  __proto__: baseController,
  get path() {
    return 'results/' + this.tag + '/bookmarks-results.json';
  },
  beforeSelect: function() {
    this.result = {};
    this.isShown = false;
    bookmarksView.setUpHandlers();
    var _t = this;
    var callback = function(obj) {
      delete _t.req;
      if (view != bookmarksView) {
        // ignore, we have switched again
        return;
      }
      _t.result = obj;
      bookmarksView.updateView(keys(_t.result));
    };
    this.req = JSON.get('results/' + this.tag + '/bookmarks-results.json', callback);
  },
  beforeUnSelect: function() {
    if (this.req) {
      this.req.abort();
      delete this.req;
    }
    bookmarksView.destroyHandlers();
  },
  showView: function(aClosure) {
    // json onload handler does this;
  },
  getContent: function(aLoc) {
    var row = view.getCell();
    var inner = document.createElement('div');
    row.appendChild(inner);
    var r = this.result[aLoc];
    var id = "bookmarks-" + aLoc;
    inner.id = id;
    var hasDetails = true;
    t = new YAHOO.widget.TreeView(inner);
    var d = this.result[aLoc];
    var innerContent = d['__title__'];
    function createChildren(aData, aParent, isOpen) {
      if ('children' in aData) {
        // Folder
        innerContent = aData['__title__'];
        if ('__desc__' in aData && aData['__desc__']) {
          innerContent += '<br><em>' + aData['__desc__'] + '</em>';
        }
        var nd = new YAHOO.widget.HTMLNode(innerContent, aParent, isOpen, true);
        for each (child in aData['children']) {
          createChildren(child, nd, true);
        }
      }
      else {
        // Link
        innerContent = '';
        if ('ICON' in aData) {
          innerContent = '<img src="' + aData['ICON'] + '">';
        }
        innerContent += '<a href="' + aData['HREF'] + '">' +
          aData['__title__'] + '</a>';
        if ('FEEDURL' in aData) {
          innerContent += '<a href="' + aData['FEEDURL'] + '">&nbsp;<em>(FEED)</em></a>';
        }
        new YAHOO.widget.HTMLNode(innerContent, aParent, false, false);
      }
    }
    createChildren(d, t.getRoot(), false);
    t.draw();
    return row;
  }
};
controller.addPane('Bookmarks', 'bookmarks', bookmarksController, 
                   bookmarksView);
