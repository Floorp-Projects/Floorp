/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   * Sets a lazy getter function for a key's value. If the key is a not a string,
   * it will be converted to a string before the set happens.
   * @param aKey
   *        The key to set
   * @param aThunk
   *        A getter function to be called the first time the value for aKey is
   *        retrieved. It is guaranteed that aThunk wouldn't be called more
   *        than once.  Note that the key value may be retrieved either
   *        directly, by |get|, or indirectly, by |listvalues| or by iterating
   *        |values|.  For the later, the value is only retrieved if and when
   *        the iterator gets to the value in question.  Also note that calling
   *        |has| for a lazy-key does not invoke aThunk.
   *
   * @note No context is provided for aThunk when it's invoked.
   *       Use Function.bind if you wish to run it in a certain context.
   */
  setAsLazyGetter: function Dict_setAsLazyGetter(aKey, aThunk) {
    let prop = convert(aKey);
    let items = this._state.items;
    if (!items.hasOwnProperty(prop))
      this._state.count++;

    Object.defineProperty(items, prop, {
      get: function() {
        delete items[prop];
        return items[prop] = aThunk();
      },
      configurable: true,
      enumerable: true     
    });
  },

  /**
   * Returns whether a key is set as a lazy getter.  This returns
   * true only if the getter function was not called already.
   * @param aKey
   *        The key to look up.
   * @returns whether aKey is set as a lazy getter.
   */
  isLazyGetter: function Dict_isLazyGetter(aKey) {
    let descriptor = Object.getOwnPropertyDescriptor(this._state.items,
                                                     convert(aKey));
    return (descriptor && descriptor.get != null);
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
