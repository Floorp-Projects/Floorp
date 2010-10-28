/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is DevTools (HeadsUpDisplay) Console Code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Julian Viereck <jviereck@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["PropertyPanel", "PropertyTreeView", "namesAndValuesOf"];

///////////////////////////////////////////////////////////////////////////
//// Helper for PropertyTreeView

const TYPE_OBJECT = 0, TYPE_FUNCTION = 1, TYPE_ARRAY = 2, TYPE_OTHER = 3;

/**
 * Figures out the type of aObject and the string to display in the tree.
 *
 * @param object aObject
 *        The object to operate on.
 * @returns object
 *          A object with the form:
 *            {
 *              type: TYPE_OBJECT || TYPE_FUNCTION || TYPE_ARRAY || TYPE_OTHER,
 *              display: string for displaying the object in the tree
 *            }
 */
function presentableValueFor(aObject)
{
  if (aObject === null || aObject === undefined) {
    return {
      type: TYPE_OTHER,
      display: aObject === undefined ? "undefined" : "null"
    };
  }

  let presentable;
  switch (aObject.constructor && aObject.constructor.name) {
    case "Array":
      return {
        type: TYPE_ARRAY,
        display: "Array"
      };

    case "String":
      return {
        type: TYPE_OTHER,
        display: "\"" + aObject + "\""
      };

    case "Date":
    case "RegExp":
    case "Number":
    case "Boolean":
      return {
        type: TYPE_OTHER,
        display: aObject
      };

    case "Function":
      presentable = aObject.toString();
      return {
        type: TYPE_FUNCTION,
        display: presentable.substring(0, presentable.indexOf(')') + 1)
      };

    default:
      presentable = aObject.toString();
      let m = /^\[object (\S+)\]/.exec(presentable);
      let display;

      return {
        type: TYPE_OBJECT,
        display: m ? m[1] : "Object"
      };
  }
}

/**
 * Get an array of property name value pairs for the tree.
 *
 * @param object aObject
 *        The object to get properties for.
 * @returns array of object
 *          Objects have the name, value, display, type, children properties.
 */
function namesAndValuesOf(aObject)
{
  let pairs = [];
  let value, presentable;

  for (var propName in aObject) {
    try {
      value = aObject[propName];
      presentable = presentableValueFor(value);
    }
    catch (ex) {
      continue;
    }

    let pair = {};
    pair.name = propName;
    pair.display = propName + ": " + presentable.display;
    pair.type = presentable.type;
    pair.value = value;

    // Convert the pair.name to a number for later sorting.
    pair.nameNumber = parseFloat(pair.name)
    if (isNaN(pair.nameNumber)) {
      pair.nameNumber = false;
    }

    pairs.push(pair);
  }

  pairs.sort(function(a, b)
  {
    // Sort numbers.
    if (a.nameNumber !== false && b.nameNumber === false) {
      return -1;
    }
    else if (a.nameNumber === false && b.nameNumber !== false) {
      return 1;
    }
    else if (a.nameNumber !== false && b.nameNumber !== false) {
      return a.nameNumber - b.nameNumber;
    }
    // Sort string.
    else if (a.name < b.name) {
      return -1;
    }
    else if (a.name > b.name) {
      return 1;
    }
    else {
      return 0;
    }
  });

  return pairs;
}

///////////////////////////////////////////////////////////////////////////
//// PropertyTreeView.


/**
 * This is an implementation of the nsITreeView interface. For comments on the
 * interface properties, see the documentation:
 * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsITreeView
 */
var PropertyTreeView = function() {
  this._rows = [];
};

PropertyTreeView.prototype = {

  /**
   * Stores the visible rows of the tree.
   */
  _rows: null,

  /**
   * Stores the nsITreeBoxObject for this tree.
   */
  _treeBox: null,

  /**
   * Use this setter to update the content of the tree.
   *
   * @param object aObject
   *        The new object to be displayed in the tree.
   * @returns void
   */
  set data(aObject) {
    let oldLen = this._rows.length;
    this._rows = this.getChildItems(aObject, true);
    if (this._treeBox) {
      this._treeBox.beginUpdateBatch();
      if (oldLen) {
        this._treeBox.rowCountChanged(0, -oldLen);
      }
      this._treeBox.rowCountChanged(0, this._rows.length);
      this._treeBox.endUpdateBatch();
    }
  },

  /**
   * Generates the child items for the treeView of a given aItem. If there is
   * already a children property on the aItem, this cached one is returned.
   *
   * @param object aItem
   *        An item of the tree's elements to generate the children for.
   * @param boolean aRootElement
   *        If set, aItem is handled as an JS object and not as an item
   *        element of the tree.
   * @returns array of objects
   *        Child items of aItem.
   */
  getChildItems: function(aItem, aRootElement)
  {
    // If item.children is an array, then the children has already been
    // computed and can get returned directly.
    // Skip this checking if aRootElement is true. It could happen, that aItem
    // is passed as ({children:[1,2,3]}) which would be true, although these
    // "kind" of children has no value/type etc. data as needed to display in
    // the tree. As the passed ({children:[1,2,3]}) are instanceof
    // itsWindow.Array and not this modules's global Array
    // aItem.children instanceof Array can't be true, but for saftey the
    // !aRootElement is kept here.
    if (!aRootElement && aItem && aItem.children instanceof Array) {
      return aItem.children;
    }

    let pairs;
    let newPairLevel;

    if (!aRootElement) {
      newPairLevel = aItem.level + 1;
      aItem = aItem.value;
    }
    else {
      newPairLevel = 0;
    }

    pairs = namesAndValuesOf(aItem);

    for each (var pair in pairs) {
      pair.level = newPairLevel;
      pair.isOpened = false;
      pair.children = pair.type == TYPE_OBJECT || pair.type == TYPE_FUNCTION ||
                      pair.type == TYPE_ARRAY;
    }

    return pairs;
  },

  /** nsITreeView interface implementation **/

  selection: null,

  get rowCount()                     { return this._rows.length; },
  setTree: function(treeBox)         { this._treeBox = treeBox;  },
  getCellText: function(idx, column) { return this._rows[idx].display; },
  getLevel: function(idx)            { return this._rows[idx].level; },
  isContainer: function(idx)         { return !!this._rows[idx].children; },
  isContainerOpen: function(idx)     { return this._rows[idx].isOpened; },
  isContainerEmpty: function(idx)    { return false; },
  isSeparator: function(idx)         { return false; },
  isSorted: function()               { return false; },
  isEditable: function(idx, column)  { return false; },
  isSelectable: function(row, col)   { return true; },

  getParentIndex: function(idx)
  {
    if (this.getLevel(idx) == 0) {
      return -1;
    }
    for (var t = idx - 1; t >= 0 ; t--) {
      if (this.isContainer(t)) {
        return t;
      }
    }
    return -1;
  },

  hasNextSibling: function(idx, after)
  {
    var thisLevel = this.getLevel(idx);
    return this._rows.slice(after + 1).some(function (r) r.level == thisLevel);
  },

  toggleOpenState: function(idx)
  {
    var item = this._rows[idx];
    if (!item.children) {
      return;
    }

    this._treeBox.beginUpdateBatch();
    if (item.isOpened) {
      item.isOpened = false;

      var thisLevel = item.level;
      var t = idx + 1, deleteCount = 0;
      while (t < this._rows.length && this.getLevel(t++) > thisLevel) {
        deleteCount++;
      }

      if (deleteCount) {
        this._rows.splice(idx + 1, deleteCount);
        this._treeBox.rowCountChanged(idx + 1, -deleteCount);
      }
    }
    else {
      item.isOpened = true;

      var toInsert = this.getChildItems(item);
      item.children = toInsert;
      this._rows.splice.apply(this._rows, [idx + 1, 0].concat(toInsert));

      this._treeBox.rowCountChanged(idx + 1, toInsert.length);
    }
    this._treeBox.invalidateRow(idx);
    this._treeBox.endUpdateBatch();
  },

  getImageSrc: function(idx, column) { },
  getProgressMode : function(idx,column) { },
  getCellValue: function(idx, column) { },
  cycleHeader: function(col, elem) { },
  selectionChanged: function() { },
  cycleCell: function(idx, column) { },
  performAction: function(action) { },
  performActionOnCell: function(action, index, column) { },
  performActionOnRow: function(action, row) { },
  getRowProperties: function(idx, column, prop) { },
  getCellProperties: function(idx, column, prop) { },
  getColumnProperties: function(column, element, prop) { },

  setCellValue: function(row, col, value)               { },
  setCellText: function(row, col, value)                { },
  drop: function(index, orientation, dataTransfer)      { },
  canDrop: function(index, orientation, dataTransfer)   { return false; }
};

///////////////////////////////////////////////////////////////////////////
//// Helper for creating the panel.

/**
 * Creates a DOMNode and sets all the attributes of aAttributes on the created
 * element.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 * @returns nsIDOMNode
 */
function createElement(aDocument, aTag, aAttributes)
{
  let node = aDocument.createElement(aTag);
  for (var attr in aAttributes) {
    node.setAttribute(attr, aAttributes[attr]);
  }
  return node;
}

/**
 * Creates a new DOMNode and appends it to aParent.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param nsIDOMNode aParent
 *        A parent node to append the created element.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 * @returns nsIDOMNode
 */
function appendChild(aDocument, aParent, aTag, aAttributes)
{
  let node = createElement(aDocument, aTag, aAttributes);
  aParent.appendChild(node);
  return node;
}

///////////////////////////////////////////////////////////////////////////
//// PropertyPanel

/**
 * Creates a new PropertyPanel.
 *
 * @param nsIDOMNode aParent
 *        Parent node to append the created panel to.
 * @param nsIDOMDocument aDocument
 *        Document to create the new nodes on.
 * @param string aTitle
 *        Title for the panel.
 * @param string aObject
 *        Object to display in the tree.
 * @param array of objects aButtons
 *        Array with buttons to display at the bottom of the panel.
 */
function PropertyPanel(aParent, aDocument, aTitle, aObject, aButtons)
{
  // Create the underlying panel
  this.panel = createElement(aDocument, "panel", {
    label: aTitle,
    titlebar: "normal",
    noautofocus: "true",
    noautohide: "true"
  });

  // Create the tree.
  let tree = this.tree = createElement(aDocument, "tree", {
    flex: 1,
    hidecolumnpicker: "true"
  });

  let treecols = aDocument.createElement("treecols");
  appendChild(aDocument, treecols, "treecol", {
    primary: "true",
    flex: 1,
    hideheader: "true",
    ignoreincolumnpicker: "true"
  });
  tree.appendChild(treecols);

  tree.appendChild(aDocument.createElement("treechildren"));
  this.panel.appendChild(tree);

  // Create the footer.
  let footer = createElement(aDocument, "hbox", { align: "end" });
  appendChild(aDocument, footer, "spacer", { flex: 1 });

  // The footer can have butttons.
  let self = this;
  if (aButtons) {
    aButtons.forEach(function(button) {
      let buttonNode = appendChild(aDocument, footer, "button", {
        label: button.label,
        accesskey: button.accesskey || "",
        class: button.class || "",
      });
      buttonNode.addEventListener("command", button.oncommand, false);
    });
  }

  appendChild(aDocument, footer, "resizer", { dir: "bottomend" });
  this.panel.appendChild(footer);

  aParent.appendChild(this.panel);

  // Create the treeView object.
  this.treeView = new PropertyTreeView();
  this.treeView.data = aObject;

  // Set the treeView object on the tree view. This has to be done *after* the
  // panel is shown. This is because the tree binding must be attached first.
  this.panel.addEventListener("popupshown", function onPopupShow()
  {
    self.panel.removeEventListener("popupshown", onPopupShow, false);
    self.tree.view = self.treeView;
  }, false);
}

/**
 * Destroy the PropertyPanel. This closes the poped up panel and removes
 * it from the browser DOM.
 *
 * @returns void
 */
PropertyPanel.prototype.destroy = function PP_destroy()
{
  this.panel.hidePopup();
  this.panel.parentNode.removeChild(this.panel);
  this.treeView = null;
  this.panel = null;
  this.tree = null;
}
