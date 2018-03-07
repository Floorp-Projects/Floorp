// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteAddonsChild"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Similar to Python. Returns dict[key] if it exists. Otherwise,
// sets dict[key] to default_ and returns default_.
function setDefault(dict, key, default_) {
  if (key in dict) {
    return dict[key];
  }
  dict[key] = default_;
  return default_;
}

// This code keeps track of a set of paths of the form [component_1,
// ..., component_n]. The components can be strings or booleans. The
// child is notified whenever a path is added or removed, and new
// children can request the current set of paths. The purpose is to
// keep track of all the observers and events that the child should
// monitor for the parent.
//
// In the child, clients can watch for changes to all paths that start
// with a given component.
var NotificationTracker = {
  init() {
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    cpmm.addMessageListener("Addons:ChangeNotification", this);
    this._paths = cpmm.initialProcessData.remoteAddonsNotificationPaths;
    this._registered = new Map();
    this._watchers = {};
  },

  receiveMessage(msg) {
    let path = msg.data.path;
    let count = msg.data.count;

    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }

    tracked._count = count;

    if (this._watchers[path[0]]) {
      for (let watcher of this._watchers[path[0]]) {
        this.runCallback(watcher, path, count);
      }
    }
  },

  runCallback(watcher, path, count) {
    let pathString = path.join("/");
    let registeredSet = this._registered.get(watcher);
    let registered = registeredSet.has(pathString);
    if (count && !registered) {
      watcher.track(path, true);
      registeredSet.add(pathString);
    } else if (!count && registered) {
      watcher.track(path, false);
      registeredSet.delete(pathString);
    }
  },

  findPaths(prefix) {
    if (!this._paths) {
      return [];
    }

    let tracked = this._paths;
    for (let component of prefix) {
      tracked = setDefault(tracked, component, {});
    }

    let result = [];
    let enumerate = (tracked, curPath) => {
      for (let component in tracked) {
        if (component == "_count") {
          result.push([curPath, tracked._count]);
        } else {
          let path = curPath.slice();
          if (component === "true") {
            component = true;
          } else if (component === "false") {
            component = false;
          }
          path.push(component);
          enumerate(tracked[component], path);
        }
      }
    };
    enumerate(tracked, prefix);

    return result;
  },

  findSuffixes(prefix) {
    let paths = this.findPaths(prefix);
    return paths.map(([path, count]) => path[path.length - 1]);
  },

  watch(component1, watcher) {
    setDefault(this._watchers, component1, []).push(watcher);
    this._registered.set(watcher, new Set());

    let paths = this.findPaths([component1]);
    for (let [path, count] of paths) {
      this.runCallback(watcher, path, count);
    }
  },

  unwatch(component1, watcher) {
    let watchers = this._watchers[component1];
    let index = watchers.lastIndexOf(watcher);
    if (index > -1) {
      watchers.splice(index, 1);
    }

    this._registered.delete(watcher);
  },

  getCount(component1) {
    return this.findPaths([component1]).length;
  },
};


var RemoteAddonsChild = {
  _ready: false,

  makeReady() {
  },

  init(global) {

    if (!this._ready) {
      if (!Services.cpmm.initialProcessData.remoteAddonsParentInitted) {
        return null;
      }

      this.makeReady();
      this._ready = true;
    }

    global.sendAsyncMessage("Addons:RegisterGlobal", {}, {global});

    return [];
  },

  uninit(perTabShims) {
    for (let shim of perTabShims) {
      try {
        shim.uninit();
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  get useSyncWebProgress() {
    return NotificationTracker.getCount("web-progress") > 0;
  },
};
