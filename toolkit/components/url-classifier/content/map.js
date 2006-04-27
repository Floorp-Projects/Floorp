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


// This is a map over which you can iterate (you can't get an iterator
// for an Object used as a map). It is important to have an iterable
// map if you have a large numbe ofkeys and you wish to process them
// in chunks, for example on a thread.
//
// This Map's operations are all O(1) plus the time of the native
// operations on its arrays and object. The tradeoff for this speed is
// that the size of the map is always non-decreasing. That is, when an 
// item is erase()'d, it is merely marked as invalid and not actually 
// removed.
// 
// This map is not safe if your keys are objects (they'll get
// stringified to the same value in non-debug builds).
// 
// Interface:
//   insert(key, value)
//   erase(key)
//   find(key)
//   getList()           // This is our poor man's iterator
//   forEach(someFunc)
//   replace(otherMap)   // Clones otherMap, replacing the current map
//   size                // readonly attribute
//
// TODO: we could easily have this map periodically compact itself so as
//       to eliminate the size tradeoff.

/**
 * Create a new Map
 *
 * @constructor
 * @param opt_name A string used to name the map
 */
function G_Map(opt_name) {
  this.debugZone = "map";
  this.name = "noname";
  
  // We store key/value pairs sequentially in membersArray
  this.membersArray_ = [];

  // In membersMap we map from keys to indices in the membersArray 
  this.membersMap_ = {};
  this.size_ = 0;

  // set to true to allow list manager to update
  this.needsUpdate = false;
}

/**
 * Add an item
 *
 * @param key An key to add (overwrites previous key)
 * @param value The value to store at that key
 */
G_Map.prototype.insert = function(key, value) {
  if (key == null)
    throw new Error("Can't use null as a key");
  else if (typeof key == "object")
    throw new Error("This class is not objectsafe; use ObjectSafeMap");
  if (value === undefined)
    throw new Error("Can't store undefined values in this map");

  // Get rid of the old value, if there was one, and increment size if not
  var oldIndexInArray = this.membersMap_[key];
  if (typeof oldIndexInArray == "number")
    this.membersArray_[oldIndexInArray] = undefined;
  else 
    this.size_++;

  var indexInArray = this.membersArray_.length;
  this.membersArray_.push({ "key": key, "value": value});
  this.membersMap_[key] = indexInArray;
}

/**
 * Remove a key from the map
 *
 * @param key The key to remove
 * @returns Boolean indicating if the key was removed
 */
G_Map.prototype.erase = function(key) {
  var indexInArray = this.membersMap_[key];
  
  if (indexInArray === undefined)
    return false;

  this.size_--;
  delete this.membersMap_[key];
  // We could slice here, but that could be expensive if the map large
  this.membersArray_[indexInArray] = undefined;  

  return true;
}

/**
 * Private lookup function
 *
 * @param key The key to look up
 * @returns The value at that key or undefined if it doesn't exist
 */
G_Map.prototype.find_ = function(key) {
  var indexInArray = this.membersMap_[key];
  return ((indexInArray === undefined) ? 
          undefined : 
          this.membersArray_[indexInArray].value);
}

/**
 * Public lookup function
 *
 * @param key The key to look up
 * @returns The value at that key or undefined if it doesn't exist
 */
G_Map.prototype.find = function(key) {
  return this.find_(key);
}

/**
 * Return the list of the keys we have.  Use findValue to get the associated
 * value.
 *
 * TODO: its probably better to expose a real iterator, but this works
 * TODO: getting keys and values is a pain, do something more clever
 *
 * @returns Array of items in the map, some entries of which might be
 *          undefined. Each item is an object with members key and
 *          value
 */ 
G_Map.prototype.getKeys = function(count) {
  var ret = [];
  for (var i = 0; i < this.membersArray_.length; ++i) {
    ret.push(this.membersArray_[i].key);
  }
  count.value = ret.length;
  return ret;
}

G_Map.prototype.findValue = function(key) {
  return this.find_(key);
}

/**
 * Replace one map with the content of another
 * 
 * IMPORTANT NOTE: This method copies references to objects, it does
 * NOT copy the objects themselves.
 *
 * @param other Reference to a G_Map that we should clone
 */
G_Map.prototype.replace = function(other) {
  this.membersMap_ = {};
  this.membersArray_ = [];

  // TODO Make this faster, and possibly run it on a thread
  var lst = {};
  var otherList = other.getList(lst);
  for (var i = 0; i < otherList.length; i++) 
    if (otherList[i]) {
      var index = this.membersArray_.length;
      this.membersArray_.push(otherList[i]);
      this.membersMap_[otherList[i].key] = index;
    }
  this.size_ = other.size;
}

/**
 * Apply a function to each of the key value pairs.
 *
 * @param func Function to apply to the map's key value pairs
 */
G_Map.prototype.forEach = function(func) {
  if (typeof func != "function")
    throw new Error("argument to forEach is not a function, it's a(n) " + 
                    typeof func);

  for (var i = 0; i < this.membersArray_.length; i++)
    if (this.membersArray_[i])
      func(this.membersArray_[i].key, this.membersArray_[i].value);
}

G_Map.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsISample) ||
      iid.equals(Components.interfaces.nsIUrlClassifierTable))
    return this;

  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;
}

/**
 * Readonly size attribute.
 * @returns The number of keys in the map
 */
G_Map.prototype.__defineGetter__('size', function() {
  return this.size_;
});
