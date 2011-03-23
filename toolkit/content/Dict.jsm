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
 * The Original Code is dict.js.
 * 
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *   Siddharth Agarwal <sid.bugzilla@gmail.com>
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

"use strict";

var EXPORTED_SYMBOLS = ["Dict"];

/**
 * Transforms a given key into a property name guaranteed not to collide with
 * any built-ins.
 */
function convert(aKey) {
  return ":" + aKey;
}

/**
 * Transforms a property into a key suitable for providing to the outside world.
 */
function unconvert(aProp) {
  return aProp.substr(1);
}

/**
 * A dictionary of strings to arbitrary JS objects. This should be used whenever
 * the keys are potentially arbitrary, to avoid collisions with built-in
 * properties.
 *
 * @param aInitial An object containing the initial keys and values of this
 *                 dictionary. Only the "own" enumerable properties of the
 *                 object are considered.
 */
function Dict(aInitial) {
  if (aInitial === undefined)
    aInitial = {};
  var items = {}, count = 0;
  // That we don't look up the prototype chain is guaranteed by Iterator.
  for (var [key, val] in Iterator(aInitial)) {
    items[convert(key)] = val;
    count++;
  }
  this._state = {count: count, items: items};
  return Object.freeze(this);
}

Dict.prototype = Object.freeze({
  /**
   * The number of items in the dictionary.
   */
  get count() {
    return this._state.count;
  },

  /**
   * Gets the value for a key from the dictionary. If the key is not a string,
   * it will be converted to a string before the lookup happens.
   *
   * @param aKey The key to look up
   * @param [aDefault] An optional default value to return if the key is not
   *                   present. Defaults to |undefined|.
   * @returns The item, or aDefault if it isn't found.
   */
  get: function Dict_get(aKey, aDefault) {
    var prop = convert(aKey);
    var items = this._state.items;
    return items.hasOwnProperty(prop) ? items[prop] : aDefault;
  },

  /**
   * Sets the value for a key in the dictionary. If the key is a not a string,
   * it will be converted to a string before the set happens.
   */
  set: function Dict_set(aKey, aValue) {
    var prop = convert(aKey);
    var items = this._state.items;
    if (!items.hasOwnProperty(prop))
      this._state.count++;
    items[prop] = aValue;
  },

  /**
   * Returns whether a key is in the dictionary. If the key is a not a string,
   * it will be converted to a string before the lookup happens.
   */
  has: function Dict_has(aKey) {
    return (this._state.items.hasOwnProperty(convert(aKey)));
  },

  /**
   * Deletes a key from the dictionary. If the key is a not a string, it will be
   * converted to a string before the delete happens.
   *
   * @returns true if the key was found, false if it wasn't.
   */
  del: function Dict_del(aKey) {
    var prop = convert(aKey);
    if (this._state.items.hasOwnProperty(prop)) {
      delete this._state.items[prop];
      this._state.count--;
      return true;
    }
    return false;
  },

  /**
   * Returns a shallow copy of this dictionary.
   */
  copy: function Dict_copy() {
    var newItems = {};
    for (var [key, val] in this.items)
      newItems[key] = val;
    return new Dict(newItems);
  },

  /*
   * List and iterator functions
   *
   * No guarantees whatsoever are made about the order of elements.
   */

  /**
   * Returns a list of all the keys in the dictionary in an arbitrary order.
   */
  listkeys: function Dict_listkeys() {
    return [unconvert(k) for (k in this._state.items)];
  },

  /**
   * Returns a list of all the values in the dictionary in an arbitrary order.
   */
  listvalues: function Dict_listvalues() {
    var items = this._state.items;
    return [items[k] for (k in items)];
  },

  /**
   * Returns a list of all the items in the dictionary as key-value pairs
   * in an arbitrary order.
   */
  listitems: function Dict_listitems() {
    var items = this._state.items;
    return [[unconvert(k), items[k]] for (k in items)];
  },

  /**
   * Returns an iterator over all the keys in the dictionary in an arbitrary
   * order. No guarantees are made about what happens if the dictionary is
   * mutated during iteration.
   */
  get keys() {
    // If we don't capture this._state.items here then the this-binding will be
    // incorrect when the generator is executed
    var items = this._state.items;
    return (unconvert(k) for (k in items));
  },

  /**
   * Returns an iterator over all the values in the dictionary in an arbitrary
   * order. No guarantees are made about what happens if the dictionary is
   * mutated during iteration.
   */
  get values() {
    // If we don't capture this._state.items here then the this-binding will be
    // incorrect when the generator is executed
    var items = this._state.items;
    return (items[k] for (k in items));
  },

  /**
   * Returns an iterator over all the items in the dictionary as key-value pairs
   * in an arbitrary order. No guarantees are made about what happens if the
   * dictionary is mutated during iteration.
   */
  get items() {
    // If we don't capture this._state.items here then the this-binding will be
    // incorrect when the generator is executed
    var items = this._state.items;
    return ([unconvert(k), items[k]] for (k in items));
  },

  /**
   * Returns a string representation of this dictionary.
   */
  toString: function Dict_toString() {
    return "{" +
      [(key + ": " + val) for ([key, val] in this.items)].join(", ") +
      "}";
  },
});
