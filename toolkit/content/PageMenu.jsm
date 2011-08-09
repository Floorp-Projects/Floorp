# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla code.
#
# The Initial Developer of the Original Code is Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

let EXPORTED_SYMBOLS = ["PageMenu"];

function PageMenu() {
}

PageMenu.prototype = {
  PAGEMENU_ATTR: "pagemenu",
  GENERATED_ATTR: "generated",
  IDENT_ATTR: "ident",

  popup: null,
  builder: null,

  init: function(aTarget, aPopup) {
    var pageMenu = null;
    var target = aTarget;
    while (target) {
      var contextMenu = target.contextMenu;
      if (contextMenu) {
        pageMenu = contextMenu;
        break;
      }
      target = target.parentNode;
    }

    if (!pageMenu) {
      return false;
    }

    var insertionPoint = this.getInsertionPoint(aPopup);
    if (!insertionPoint) {
      return false;
    }

    pageMenu.QueryInterface(Components.interfaces.nsIHTMLMenu);
    pageMenu.sendShowEvent();
    // the show event is not cancelable, so no need to check a result here

    var fragment = aPopup.ownerDocument.createDocumentFragment();

    var builder = pageMenu.createBuilder();
    if (!builder) {
      return false;
    }
    builder.QueryInterface(Components.interfaces.nsIXULContextMenuBuilder);
    builder.init(fragment, this.GENERATED_ATTR, this.IDENT_ATTR);

    pageMenu.build(builder);

    var pos = insertionPoint.getAttribute(this.PAGEMENU_ATTR);
    if (pos == "end") {
      insertionPoint.appendChild(fragment);
    } else {
      insertionPoint.insertBefore(fragment,
                                  insertionPoint.firstChild);
    }

    this.builder = builder;
    this.popup = aPopup;

    this.popup.addEventListener("command", this);
    this.popup.addEventListener("popuphidden", this);

    return true;
  },

  handleEvent: function(event) {
    var type = event.type;
    var target = event.target;
    if (type == "command" && target.hasAttribute(this.GENERATED_ATTR)) {
      this.builder.click(target.getAttribute(this.IDENT_ATTR));
    } else if (type == "popuphidden" && this.popup == target) {
      this.removeGeneratedContent(this.popup);

      this.popup.removeEventListener("popuphidden", this);
      this.popup.removeEventListener("command", this);

      this.popup = null;
      this.builder = null;
    }
  },

  getImmediateChild: function(element, tag) {
    var child = element.firstChild;
    while (child) {
      if (child.localName == tag) {
        return child;
      }
      child = child.nextSibling;
    }
    return null;
  },

  getInsertionPoint: function(aPopup) {
    if (aPopup.hasAttribute(this.PAGEMENU_ATTR))
      return aPopup;

    var element = aPopup.firstChild;
    while (element) {
      if (element.localName == "menu") {
        var popup = this.getImmediateChild(element, "menupopup");
        if (popup) {
          var result = this.getInsertionPoint(popup);
          if (result) {
            return result;
          }
        }
      }
      element = element.nextSibling;
    }

    return null;
  },

  removeGeneratedContent: function(aPopup) {
    var ungenerated = [];
    ungenerated.push(aPopup);

    var count;
    while (0 != (count = ungenerated.length)) {
      var last = count - 1;
      var element = ungenerated[last];
      ungenerated.splice(last, 1);

      var i = element.childNodes.length;
      while (i-- > 0) {
        var child = element.childNodes[i];
        if (!child.hasAttribute(this.GENERATED_ATTR)) {
          ungenerated.push(child);
          continue;
        }
        element.removeChild(child);
      }
    }
  }
}
