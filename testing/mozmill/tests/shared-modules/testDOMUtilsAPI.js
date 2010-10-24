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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
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

var MODULE_NAME = 'DOMUtilsAPI';


/**
 * Default constructor
 *
 * @param {object} aRoot
 *        Root node in the DOM to use as parent
 */
function nodeCollector(aRoot) {
  this._root = aRoot.wrappedJSObject ? aRoot.wrappedJSObject : aRoot;
  this._document = this._root.ownerDocument ? this._root.ownerDocument : this._root;
  this._nodes = [ ];
}

/**
 * Node collector class
 */
nodeCollector.prototype = {
  /**
   * Converts current nodes to elements
   *
   * @returns List of elements
   * @type {array of ElemBase}
   */
  get elements() {
    var elements = [ ];

    Array.forEach(this._nodes, function(element) {
      elements.push(new elementslib.Elem(element));
    });

    return elements;
  },

  /**
   * Get the current list of DOM nodes
   *
   * @returns List of nodes
   * @type {array of object}
   */
  get nodes() {
    return this._nodes;
  },

  /**
   * Sets current nodes to entries from the node list
   * 
   * @param {array of objects} aNodeList
   *        List of DOM nodes to set
   */
  set nodes(aNodeList) {
    if (aNodeList) {
      this._nodes = [ ];

      Array.forEach(aNodeList, function(node) {
        this._nodes.push(node);
      }, this);
    }
  },

  /**
   * Get the root node used as parent for a node collection
   *
   * @returns Current root node
   * @type {object}
   */
  get root() {
    return this._root;
  },

  /**
   * Sets root node to the specified DOM node
   * 
   * @param {object} aRoot
   *        DOM node to use as root for node collection
   */
  set root(aRoot) {
    if (aRoot) {
      this._root = aRoot;
      this._nodes = [ ];
    }
  },

  /**
   * Filter nodes given by the specified callback function
   *
   * @param {function} aCallback
   *        Function to test each element of the array.
   *        Elements: node, index (optional) , array (optional)
   * @param {object} aThisObject
   *        Object to use as 'this' when executing callback.
   *        [optional - default: function scope]
   *
   * @returns The class instance
   * @type {object}
   */
  filter : function nodeCollector_filter(aCallback, aThisObject) {
    if (!aCallback)
      throw new Error(arguments.callee.name + ": No callback specified");

    this.nodes = Array.filter(this.nodes, aCallback, aThisObject);

    return this;
  },

  /**
   * Filter nodes by DOM property and its value
   *
   * @param {string} aProperty
   *        Property to filter for
   * @param {string} aValue
   *        Expected value of the DOM property
   *        [optional - default: n/a]
   *
   * @returns The class instance
   * @type {object}
   */
  filterByDOMProperty : function nodeCollector_filterByDOMProperty(aProperty, aValue) {
    return this.filter(function(node) {
      if (aProperty && aValue)
        return node.getAttribute(aProperty) == aValue;
      else if (aProperty)
        return node.hasAttribute(aProperty);
      else
        return true;
    });
  },

  /**
   * Filter nodes by JS property and its value
   *
   * @param {string} aProperty
   *        Property to filter for
   * @param {string} aValue
   *        Expected value of the JS property
   *        [optional - default: n/a]
   *
   * @returns The class instance
   * @type {object}
   */
  filterByJSProperty : function nodeCollector_filterByJSProperty(aProperty, aValue) {
    return this.filter(function(node) {
      if (aProperty && aValue)
        return node.aProperty == aValue;
      else if (aProperty)
        return node.aProperty !== undefined;
      else
        return true;
    });
  },

  /**
   * Find anonymouse nodes with the specified attribute and value
   *
   * @param {string} aAttribute
   *        DOM attribute of the wanted node
   * @param {string} aValue
   *        Value of the DOM attribute
   *
   * @returns The class instance
   * @type {object}
   */
  queryAnonymousNodes : function nodeCollector_queryAnonymousNodes(aAttribute, aValue) {
    var node = this._document.getAnonymousElementByAttribute(this._root,
                                                             aAttribute,
                                                             aValue);
    this.nodes = node ? [node] : [ ];

    return this;
  },

  /**
   * Find nodes with the specified selector
   *
   * @param {string} aSelector
   *        jQuery like element selector string
   *
   * @returns The class instance
   * @type {object}
   */
  queryNodes : function nodeCollector_queryNodes(aSelector) {
    this.nodes = this._root.querySelectorAll(aSelector);

    return this;
  }
}
