// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteAddonsParent"];

ChromeUtils.import("resource://gre/modules/RemoteWebProgress.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

Cu.permitCPOWsInScope(this);

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
var NotificationTracker = {
  // _paths is a multi-level dictionary. Let's add paths [A, B] and
  // [A, C]. Then _paths will look like this:
  //   { 'A': { 'B': { '_count': 1 }, 'C': { '_count': 1 } } }
  // Each component in a path will be a key in some dictionary. At the
  // end, the _count property keeps track of how many instances of the
  // given path are present in _paths.
  _paths: {},

  init() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.initialProcessData.remoteAddonsNotificationPaths = this._paths;
  },

  add(path) {
    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }
    let count = tracked._count || 0;
    count++;
    tracked._count = count;

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.broadcastAsyncMessage("Addons:ChangeNotification", {path, count});
  },

  remove(path) {
    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }
    tracked._count--;

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.broadcastAsyncMessage("Addons:ChangeNotification", {path, count: tracked._count});
  },
};
NotificationTracker.init();

// An interposition is an object with three properties: methods,
// getters, and setters. See multiprocessShims.js for an explanation
// of how these are used. The constructor here just allows one
// interposition to inherit members from another.
function Interposition(name, base) {
  this.name = name;
  if (base) {
    this.methods = Object.create(base.methods);
    this.getters = Object.create(base.getters);
    this.setters = Object.create(base.setters);
  } else {
    this.methods = Object.create(null);
    this.getters = Object.create(null);
    this.setters = Object.create(null);
  }
}

var TabBrowserElementInterposition = new Interposition("TabBrowserElementInterposition");

// This function returns a wrapper around an
// nsIWebProgressListener. When the wrapper is invoked, it calls the
// real listener but passes CPOWs for the nsIWebProgress and
// nsIRequest arguments.
var progressListeners = {global: new WeakMap(), tabs: new WeakMap()};
function wrapProgressListener(kind, listener) {
  if (progressListeners[kind].has(listener)) {
    return progressListeners[kind].get(listener);
  }

  let ListenerHandler = {
    get(target, name) {
      if (name.startsWith("on")) {
        return function(...args) {
          listener[name].apply(listener, RemoteWebProgressManager.argumentsForAddonListener(kind, args));
        };
      }

      return listener[name];
    }
  };
  let listenerProxy = new Proxy(listener, ListenerHandler);

  progressListeners[kind].set(listener, listenerProxy);
  return listenerProxy;
}

TabBrowserElementInterposition.methods.addProgressListener = function(addon, target, listener) {
  if (!target.ownerGlobal.gMultiProcessBrowser) {
    return target.addProgressListener(listener);
  }

  NotificationTracker.add(["web-progress", addon]);
  return target.addProgressListener(wrapProgressListener("global", listener));
};

TabBrowserElementInterposition.methods.removeProgressListener = function(addon, target, listener) {
  if (!target.ownerGlobal.gMultiProcessBrowser) {
    return target.removeProgressListener(listener);
  }

  NotificationTracker.remove(["web-progress", addon]);
  return target.removeProgressListener(wrapProgressListener("global", listener));
};

var RemoteAddonsParent = {
  init() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("Addons:RegisterGlobal", this);

    Services.ppmm.initialProcessData.remoteAddonsParentInitted = true;

    Services.ppmm.loadProcessScript("data:,new " + function() {
      ChromeUtils.import("resource://gre/modules/RemoteAddonsChild.jsm");
    }, true);

    this.globalToBrowser = new WeakMap();
    this.browserToGlobal = new WeakMap();
  },

  getInterfaceInterpositions() {
    return {};
  },

  getTaggedInterpositions() {
    return {
      TabBrowserElement: TabBrowserElementInterposition,
    };
  },

  receiveMessage(msg) {
    switch (msg.name) {
    case "Addons:RegisterGlobal":
      this.browserToGlobal.set(msg.target, msg.objects.global);
      this.globalToBrowser.set(msg.objects.global, msg.target);
      break;
    }
  }
};
