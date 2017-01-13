/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FolderCache"];

// Cache for bookmarks folder heirarchy.
var FolderCache = function () {
  this.cache = new Map();
}

FolderCache.prototype = {
  has: function (id) {
    return this.cache.has(id);
  },

  insert: function (id, parentId) {
    if (this.cache.has(id)) {
      return;
    }

    if (parentId && !(this.cache.has(parentId))) {
      throw new Error("insert :: parentId not found in cache: " + parentId);
    }

    this.cache.set(id, {
      parent: parentId || null,
      children: new Set(),
    });

    if (parentId) {
      this.cache.get(parentId).children.add(id);
    }
  },

  remove: function (id) {
    if (!(this.cache.has(id))) {
      throw new Error("remote :: id not found in cache: " + id);
    }

    let parentId = this.cache.get(id).parent;
    if (parentId) {
      this.cache.get(parentId).children.delete(id);
    }

    for (let child of this.cache.get(id).children) {
      this.cache.get(child).parent = null;
    }

    this.cache.delete(id);
  },

  setParent: function (id, parentId) {
    if (!(this.cache.has(id))) {
      throw new Error("setParent :: id not found in cache: " + id);
    }

    if (parentId && !(this.cache.has(parentId))) {
      throw new Error("setParent :: parentId not found in cache: " + parentId);
    }

    let oldParent = this.cache.get(id).parent;
    if (oldParent) {
      this.cache.get(oldParent).children.delete(id);
    }
    this.cache.get(id).parent = parentId;
    this.cache.get(parentId).children.add(id);

    return true;
  },

  getParent: function (id) {
    if (this.cache.has(id)) {
      return this.cache.get(id).parent;
    }

    throw new Error("getParent :: id not found in cache: " + id);
  },

  getChildren: function (id) {
    if (this.cache.has(id)) {
      return this.cache.get(id).children;
    }

    throw new Error("getChildren :: id not found in cache: " + id);
  },

  setChildren: function (id, children) {
    for (let child of children) {
      if (!this.cache.has(child)) {
        this.insert(child, id);
      } else {
        this.setParent(child, id);
      }
    }
  },

  dump: function () {
    dump("FolderCache: " + JSON.stringify(this.cache) + "\n");
  },
};

this.FolderCache = FolderCache;
