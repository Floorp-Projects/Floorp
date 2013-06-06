/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [
  "ContentPrefStore",
];

function ContentPrefStore() {
  this._groups = {};
  this._globalNames = {};
}

ContentPrefStore.prototype = {

  set: function CPS_set(group, name, val) {
    if (group) {
      if (!this._groups.hasOwnProperty(group))
        this._groups[group] = {};
      this._groups[group][name] = val;
    }
    else {
      this._globalNames[name] = val;
    }
  },

  setWithCast: function CPS_setWithCast(group, name, val) {
    if (typeof(val) == "boolean")
      val = val ? 1 : 0;
    else if (val === undefined)
      val = null;
    this.set(group, name, val);
  },

  has: function CPS_has(group, name) {
    return (group &&
            this._groups.hasOwnProperty(group) &&
            this._groups[group].hasOwnProperty(name)) ||
           (!group &&
            this._globalNames.hasOwnProperty(name));
  },

  get: function CPS_get(group, name) {
    if (group) {
      if (this._groups.hasOwnProperty(group) &&
          this._groups[group].hasOwnProperty(name))
        return this._groups[group][name];
    }
    else if (this._globalNames.hasOwnProperty(name)) {
      return this._globalNames[name];
    }
    return undefined;
  },

  remove: function CPS_remove(group, name) {
    if (group) {
      if (this._groups.hasOwnProperty(group)) {
        delete this._groups[group][name];
        if (!Object.keys(this._groups[group]).length)
          delete this._groups[group];
      }
    }
    else {
      delete this._globalNames[name];
    }
  },

  removeGroup: function CPS_removeGroup(group) {
    if (group) {
      delete this._groups[group];
    }
    else {
      this._globalNames = {};
    }
  },

  removeAllGroups: function CPS_removeAllGroups() {
    this._groups = {};
  },

  removeAll: function CPS_removeAll() {
    this.removeAllGroups();
    this._globalNames = {};
  },

  __iterator__: function CPS___iterator__() {
    for (let [group, names] in Iterator(this._groups)) {
      for (let [name, val] in Iterator(names)) {
        yield [group, name, val];
      }
    }
    for (let [name, val] in Iterator(this._globalNames)) {
      yield [null, name, val];
    }
  },

  match: function CPS_match(group, name, includeSubdomains) {
    for (let sgroup in this.matchGroups(group, includeSubdomains)) {
      if (this.has(sgroup, name))
        yield [sgroup, this.get(sgroup, name)];
    }
  },

  matchGroups: function CPS_matchGroups(group, includeSubdomains) {
    if (group) {
      if (includeSubdomains) {
        for (let [sgroup, , ] in this) {
          if (sgroup) {
            let idx = sgroup.indexOf(group);
            if (idx == sgroup.length - group.length &&
                (idx == 0 || sgroup[idx - 1] == "."))
              yield sgroup;
          }
        }
      }
      else if (this._groups.hasOwnProperty(group)) {
        yield group;
      }
    }
    else if (Object.keys(this._globalNames).length) {
      yield null;
    }
  },
};
