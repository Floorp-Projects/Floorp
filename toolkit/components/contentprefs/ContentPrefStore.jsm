/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ContentPrefStore"];

function ContentPrefStore() {
  this._groups = new Map();
  this._globalNames = new Map();
}

ContentPrefStore.prototype = {
  set: function CPS_set(group, name, val) {
    if (group) {
      if (!this._groups.has(group)) {
        this._groups.set(group, new Map());
      }
      this._groups.get(group).set(name, val);
    } else {
      this._globalNames.set(name, val);
    }
  },

  setWithCast: function CPS_setWithCast(group, name, val) {
    if (typeof val == "boolean") {
      val = val ? 1 : 0;
    } else if (val === undefined) {
      val = null;
    }
    this.set(group, name, val);
  },

  has: function CPS_has(group, name) {
    if (group) {
      return this._groups.has(group) && this._groups.get(group).has(name);
    }
    return this._globalNames.has(name);
  },

  get: function CPS_get(group, name) {
    if (group && this._groups.has(group)) {
      return this._groups.get(group).get(name);
    }
    return this._globalNames.get(name);
  },

  remove: function CPS_remove(group, name) {
    if (group) {
      if (this._groups.has(group)) {
        this._groups.get(group).delete(name);
        if (this._groups.get(group).size == 0) {
          this._groups.delete(group);
        }
      }
    } else {
      this._globalNames.delete(name);
    }
  },

  removeGroup: function CPS_removeGroup(group) {
    if (group) {
      this._groups.delete(group);
    } else {
      this._globalNames.clear();
    }
  },

  removeAllGroups: function CPS_removeAllGroups() {
    this._groups.clear();
  },

  removeAll: function CPS_removeAll() {
    this.removeAllGroups();
    this._globalNames.clear();
  },

  groupsMatchIncludingSubdomains: function CPS_groupsMatchIncludingSubdomains(
    group,
    group2
  ) {
    let idx = group2.indexOf(group);
    return (
      idx == group2.length - group.length &&
      (idx == 0 || group2[idx - 1] == ".")
    );
  },

  *[Symbol.iterator]() {
    for (let [group, names] of this._groups) {
      for (let [name, val] of names) {
        yield [group, name, val];
      }
    }
    for (let [name, val] of this._globalNames) {
      yield [null, name, val];
    }
  },

  *match(group, name, includeSubdomains) {
    for (let sgroup of this.matchGroups(group, includeSubdomains)) {
      if (this.has(sgroup, name)) {
        yield [sgroup, this.get(sgroup, name)];
      }
    }
  },

  *matchGroups(group, includeSubdomains) {
    if (group) {
      if (includeSubdomains) {
        for (let [sgroup, ,] of this) {
          if (sgroup) {
            if (this.groupsMatchIncludingSubdomains(group, sgroup)) {
              yield sgroup;
            }
          }
        }
      } else if (this._groups.has(group)) {
        yield group;
      }
    } else if (this._globalNames.size) {
      yield null;
    }
  },
};
