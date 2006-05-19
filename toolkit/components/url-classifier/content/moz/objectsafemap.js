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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// ObjectSafeMap is, shockingly, a Map with which it is safe to use
// objects as keys. It currently uses parallel arrays for storage,
// rendering it inefficient (linear) for large maps. We can always
// swap out the implementation if this becomes a problem. Note that
// this class uses strict equality to determine equivalent keys.
// 
// Interface:
//
//   insert(key, value)
//   erase(key)          // Returns true if key erased, false if not found
//   find(key)           // Returns undefined if key not found
//   replace(otherMap)   // Clones otherMap, replacing the current map
//   forEach(func)
//   size()              // Returns number of items in the map
//
// TODO: should probably make it iterable by implementing getList();


/**
 * Create a new ObjectSafeMap.
 *
 * @param opt_name A string used to name the map
 *
 * @constructor
 */
function G_ObjectSafeMap(opt_name) {
  this.debugZone = "objectsafemap";
  this.name_ = opt_name ? opt_name : "noname";
  this.keys_ = [];
  this.values_ = [];
}

/**
 * Helper function to return the index of a key. 
 *
 * @param key An key to find
 *
 * @returns Index in the keys array where the key is found, -1 if not
 */
G_ObjectSafeMap.prototype.indexOfKey_ = function(key) {
  for (var i = 0; i < this.keys_.length; i++)
    if (this.keys_[i] === key)
      return i;
  return -1;
}

/**
 * Add an item
 *
 * @param key An key to add (overwrites previous key)
 *
 * @param value The value to store at that key
 */
G_ObjectSafeMap.prototype.insert = function(key, value) {
  if (key === null)
    throw new Error("Can't use null as a key");
  if (value === undefined)
    throw new Error("Can't store undefined values in this map");

  var i = this.indexOfKey_(key);
  if (i == -1) {
    this.keys_.push(key);
    this.values_.push(value);
  } else {
    this.keys_[i] = key;
    this.values_[i] = value;
  }

  G_Assert(this, this.keys_.length == this.values_.length, 
           "Different number of keys than values!");
}

/**
 * Remove a key from the map
 *
 * @param key The key to remove
 *
 * @returns Boolean indicating if the key was removed
 */
G_ObjectSafeMap.prototype.erase = function(key) {
  var keyLocation = this.indexOfKey_(key);
  var keyFound = keyLocation != -1;
  if (keyFound) {
    this.keys_.splice(keyLocation, 1);
    this.values_.splice(keyLocation, 1);
  }
  G_Assert(this, this.keys_.length == this.values_.length, 
           "Different number of keys than values!");

  return keyFound;
}

/**
 * Look up a key in the map
 *
 * @param key The key to look up
 * 
 * @returns The value at that key or undefined if it doesn't exist
 */
G_ObjectSafeMap.prototype.find = function(key) {
  var keyLocation = this.indexOfKey_(key);
  return keyLocation == -1 ? undefined : this.values_[keyLocation];
}

/**
 * Replace one map with the content of another
 *
 * @param map input map that needs to be merged into our map
 */
G_ObjectSafeMap.prototype.replace = function(other) {
  this.keys_ = [];
  this.values_ = [];
  for (var i = 0; i < other.keys_.length; i++) {
    this.keys_.push(other.keys_[i]);
    this.values_.push(other.values_[i]);
  }

  G_Assert(this, this.keys_.length == this.values_.length, 
           "Different number of keys than values!");
}

/**
 * Apply a function to each of the key value pairs.
 *
 * @param func Function to apply to the map's key value pairs
 */
G_ObjectSafeMap.prototype.forEach = function(func) {
  if (typeof func != "function")
    throw new Error("argument to forEach is not a function, it's a(n) " + 
                    typeof func);

  for (var i = 0; i < this.keys_.length; i++)
    func(this.keys_[i], this.values_[i]);
}

/**
 * @returns The number of keys in the map
 */
G_ObjectSafeMap.prototype.size = function() {
  return this.keys_.length;
}

#ifdef DEBUG
// Cheesey, yes, but it's what we've got
function TEST_G_ObjectSafeMap() {
  if (G_GDEBUG) {
    var z = "map UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    var m = new G_ObjectSafeMap();
    G_Assert(z, m.size() == 0, "Initial size not zero");

    var o1 = new Object;
    var v1 = "1";
    var o2 = new Object;
    var v2 = "1";

    G_Assert(z, m.find(o1) == undefined, "Found non-existent item");

    m.insert(o1, v1);
    m.insert(o2, v2);

    G_Assert(z, m.size() == 2, "Size not 2");
    G_Assert(z, m.find(o1) == "1", "Didn't find item 1");
    G_Assert(z, m.find(o2) == "1", "Didn't find item 1");

    m.insert(o1, "2");

    G_Assert(z, m.size() == 2, "Size not 2");
    G_Assert(z, m.find(o1) == "2", "Didn't find item 1");
    G_Assert(z, m.find(o2) == "1", "Didn't find item 1");

    m.erase(o1);

    G_Assert(z, m.size() == 1, "Size not 1");
    G_Assert(z, m.find(o1) == undefined, "Found item1");
    G_Assert(z, m.find(o2) == "1", "Didn't find item 2");

    m.erase(o1);

    G_Assert(z, m.size() == 1, "Size not 1");
    G_Assert(z, m.find(o1) == undefined, "Found item1");
    G_Assert(z, m.find(o2) == "1", "Didn't find item 2");

    m.erase(o2);

    G_Assert(z, m.size() == 0, "Size not 0");
    G_Assert(z, m.find(o2) == undefined, "Found item2");

    G_Debug(z, "PASSED");
  }
}
#endif
