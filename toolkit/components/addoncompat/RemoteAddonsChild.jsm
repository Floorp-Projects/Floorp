// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteAddonsChild"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Similar to Python. Returns dict[key] if it exists. Otherwise,
// sets dict[key] to default_ and returns default_.
function setDefault(dict, key, default_)
{
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
let NotificationTracker = {
  init: function() {
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    cpmm.addMessageListener("Addons:AddNotification", this);
    cpmm.addMessageListener("Addons:RemoveNotification", this);
    let [paths] = cpmm.sendSyncMessage("Addons:GetNotifications");
    this._paths = paths;
    this._watchers = {};
  },

  receiveMessage: function(msg) {
    let path = msg.data;

    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }
    let count = tracked._count || 0;

    switch (msg.name) {
    case "Addons:AddNotification":
      count++;
      break;
    case "Addons:RemoveNotification":
      count--;
      break;
    }

    tracked._count = count;

    for (let cb of this._watchers[path[0]]) {
      cb(path, count);
    }
  },

  watch: function(component1, callback) {
    setDefault(this._watchers, component1, []).push(callback);

    function enumerate(tracked, curPath) {
      for (let component in tracked) {
        if (component == "_count") {
          callback(curPath, tracked._count);
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
    }
    enumerate(this._paths[component1] || {}, [component1]);
  }
};

// This code registers an nsIContentPolicy in the child process. When
// it runs, it notifies the parent that it needs to run its own
// nsIContentPolicy list. If any policy in the parent rejects a
// resource load, that answer is returned to the child.
let ContentPolicyChild = {
  _classDescription: "Addon shim content policy",
  _classID: Components.ID("6e869130-635c-11e2-bcfd-0800200c9a66"),
  _contractID: "@mozilla.org/addon-child/policy;1",

  init: function() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(this._classID, this._classDescription, this._contractID, this);

    NotificationTracker.watch("content-policy", (path, count) => this.track(path, count));
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy, Ci.nsIObserver,
                                         Ci.nsIChannelEventSink, Ci.nsIFactory,
                                         Ci.nsISupportsWeakReference]),

  track: function(path, count) {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    if (count) {
      catMan.addCategoryEntry("content-policy", this._contractID, this._contractID, false, true);
    } else {
      catMan.deleteCategoryEntry("content-policy", this._contractID, false);
    }
  },

  shouldLoad: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    var rval = cpmm.sendRpcMessage("Addons:ContentPolicy:Run", {}, {
      contentType: contentType,
      mimeTypeGuess: mimeTypeGuess,
      contentLocation: contentLocation,
      requestOrigin: requestOrigin,
      node: node
    });
    if (rval.length != 1) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    return rval[0];
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, insecNode, mimeType, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },

  createInstance: function(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },
};

// This code registers observers in the child whenever an add-on in
// the parent asks for notifications on the given topic.
let ObserverChild = {
  init: function() {
    NotificationTracker.watch("observer", (path, count) => this.track(path, count));
  },

  track: function(path, count) {
    let topic = path[1];
    if (count) {
      Services.obs.addObserver(this, topic, false);
    } else {
      Services.obs.removeObserver(this, topic);
    }
  },

  observe: function(subject, topic, data) {
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    cpmm.sendRpcMessage("Addons:Observer:Run", {}, {
      topic: topic,
      subject: subject,
      data: data
    });
  }
};

// There is one of these objects per browser tab in the child. When an
// add-on in the parent listens for an event, this child object
// listens for that event in the child.
function EventTargetChild(childGlobal)
{
  this._childGlobal = childGlobal;
  NotificationTracker.watch("event", (path, count) => this.track(path, count));
}

EventTargetChild.prototype = {
  track: function(path, count) {
    let eventType = path[1];
    let useCapture = path[2];
    if (count) {
      this._childGlobal.addEventListener(eventType, this, useCapture, true);
    } else {
      this._childGlobal.removeEventListener(eventType, this, useCapture);
    }
  },

  handleEvent: function(event) {
    this._childGlobal.sendRpcMessage("Addons:Event:Run",
                                     {type: event.type, isTrusted: event.isTrusted},
                                     {event: event});
  }
};

// The parent can create a sandbox to run code in the child
// process. We actually create the sandbox in the child so that the
// code runs there. However, managing the lifetime of these sandboxes
// can be tricky. The parent references these sandboxes using CPOWs,
// which only keep weak references. So we need to create a strong
// reference in the child. For simplicity, we kill off these strong
// references whenever we navigate away from the page for which the
// sandbox was created.
function SandboxChild(chromeGlobal)
{
  this.chromeGlobal = chromeGlobal;
  this.sandboxes = [];
}

SandboxChild.prototype = {
  addListener: function() {
    let webProgress = this.chromeGlobal.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  },

  removeListener: function() {
    let webProgress = this.chromeGlobal.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  },

  onLocationChange: function(webProgress, request, location, flags) {
    if (this.sandboxes.length) {
      this.removeListener();
    }
    this.sandboxes = [];
  },

  addSandbox: function(sandbox) {
    if (this.sandboxes.length == 0) {
      this.addListener();
    }
    this.sandboxes.push(sandbox);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};

let RemoteAddonsChild = {
  _ready: false,

  makeReady: function() {
    NotificationTracker.init();
    ContentPolicyChild.init();
    ObserverChild.init();
  },

  init: function(global) {
    if (!this._ready) {
      this.makeReady();
      this._ready = true;
    }

    global.sendAsyncMessage("Addons:RegisterGlobal", {}, {global: global});

    let sandboxChild = new SandboxChild(global);
    global.addSandbox = sandboxChild.addSandbox.bind(sandboxChild);

    // Return this so it gets rooted in the content script.
    return [new EventTargetChild(global), sandboxChild];
  },
};
