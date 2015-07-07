// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteAddonsChild"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Prefetcher",
                                  "resource://gre/modules/Prefetcher.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "SystemPrincipal",
                                   "@mozilla.org/systemprincipal;1", "nsIPrincipal");

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
    cpmm.addMessageListener("Addons:ChangeNotification", this);
    let [paths] = cpmm.sendSyncMessage("Addons:GetNotifications");
    this._paths = paths;
    this._registered = new Map();
    this._watchers = {};
  },

  receiveMessage: function(msg) {
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

  runCallback: function(watcher, path, count) {
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

  findPaths: function(prefix) {
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
    }
    enumerate(tracked, prefix);

    return result;
  },

  findSuffixes: function(prefix) {
    let paths = this.findPaths(prefix);
    return paths.map(([path, count]) => path[path.length - 1]);
  },

  watch: function(component1, watcher) {
    setDefault(this._watchers, component1, []).push(watcher);
    this._registered.set(watcher, new Set());

    let paths = this.findPaths([component1]);
    for (let [path, count] of paths) {
      this.runCallback(watcher, path, count);
    }
  },

  unwatch: function(component1, watcher) {
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

    NotificationTracker.watch("content-policy", this);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy, Ci.nsIObserver,
                                         Ci.nsIChannelEventSink, Ci.nsIFactory,
                                         Ci.nsISupportsWeakReference]),

  track: function(path, register) {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    if (register) {
      catMan.addCategoryEntry("content-policy", this._contractID, this._contractID, false, true);
    } else {
      catMan.deleteCategoryEntry("content-policy", this._contractID, false);
    }
  },

  shouldLoad: function(contentType, contentLocation, requestOrigin,
                       node, mimeTypeGuess, extra, requestPrincipal) {
    let addons = NotificationTracker.findSuffixes(["content-policy"]);
    let [prefetched, cpows] = Prefetcher.prefetch("ContentPolicy.shouldLoad",
                                                  addons, {InitNode: node});
    cpows.node = node;

    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    let rval = cpmm.sendRpcMessage("Addons:ContentPolicy:Run", {
      contentType: contentType,
      contentLocation: contentLocation.spec,
      requestOrigin: requestOrigin ? requestOrigin.spec : null,
      mimeTypeGuess: mimeTypeGuess,
      requestPrincipal: requestPrincipal,
      prefetched: prefetched,
    }, cpows);
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

// This is a shim channel whose only purpose is to return some string
// data from an about: protocol handler.
function AboutProtocolChannel(uri, contractID, loadInfo)
{
  this.URI = uri;
  this.originalURI = uri;
  this._contractID = contractID;
  this._loadingPrincipal = loadInfo.loadingPrincipal;
  this._securityFlags = loadInfo.securityFlags;
  this._contentPolicyType = loadInfo.contentPolicyType;
}

AboutProtocolChannel.prototype = {
  contentCharset: "utf-8",
  contentLength: 0,
  owner: SystemPrincipal,
  securityInfo: null,
  notificationCallbacks: null,
  loadFlags: 0,
  loadGroup: null,
  name: null,
  status: Cr.NS_OK,

  asyncOpen: function(listener, context) {
    // Ask the parent to synchronously read all the data from the channel.
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    let rval = cpmm.sendRpcMessage("Addons:AboutProtocol:OpenChannel", {
      uri: this.URI.spec,
      contractID: this._contractID,
      loadingPrincipal: this._loadingPrincipal,
      securityFlags: this._securityFlags,
      contentPolicyType: this._contentPolicyType
    }, {
      notificationCallbacks: this.notificationCallbacks,
      loadGroupNotificationCallbacks: this.loadGroup ? this.loadGroup.notificationCallbacks : null,
    });

    if (rval.length != 1) {
      throw Cr.NS_ERROR_FAILURE;
    }

    let {data, contentType} = rval[0];
    this.contentType = contentType;

    // Return the data via an nsIStringInputStream.
    let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
    stream.setData(data, data.length);

    let runnable = {
      run: () => {
        try {
          listener.onStartRequest(this, context);
        } catch(e) {}
        try {
          listener.onDataAvailable(this, context, stream, 0, stream.available());
        } catch(e) {}
        try {
          listener.onStopRequest(this, context, Cr.NS_OK);
        } catch(e) {}
      }
    };
    Services.tm.currentThread.dispatch(runnable, Ci.nsIEventTarget.DISPATCH_NORMAL);
  },

  open: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  isPending: function() {
    return false;
  },

  cancel: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  suspend: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  resume: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel, Ci.nsIRequest])
};

// This shim protocol handler is used when content fetches an about: URL.
function AboutProtocolInstance(contractID)
{
  this._contractID = contractID;
  this._uriFlags = undefined;
}

AboutProtocolInstance.prototype = {
  createInstance: function(outer, iid) {
    if (outer != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    return this.QueryInterface(iid);
  },

  getURIFlags: function(uri) {
    // Cache the result to avoid the extra IPC.
    if (this._uriFlags !== undefined) {
      return this._uriFlags;
    }

    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);

    let rval = cpmm.sendRpcMessage("Addons:AboutProtocol:GetURIFlags", {
      uri: uri.spec,
      contractID: this._contractID
    });

    if (rval.length != 1) {
      throw Cr.NS_ERROR_FAILURE;
    }

    this._uriFlags = rval[0];
    return this._uriFlags;
  },

  // We take some shortcuts here. Ideally, we would return a CPOW that
  // wraps the add-on's nsIChannel. However, many of the methods
  // related to nsIChannel are marked [noscript], so they're not
  // available to CPOWs. Consequently, we return a shim channel that,
  // when opened, asks the parent to open the channel and read out all
  // the data.
  newChannel: function(uri, loadInfo) {
    return new AboutProtocolChannel(uri, this._contractID, loadInfo);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory, Ci.nsIAboutModule])
};

let AboutProtocolChild = {
  _classDescription: "Addon shim about: protocol handler",

  init: function() {
    // Maps contractIDs to instances
    this._instances = new Map();
    // Maps contractIDs to classIDs
    this._classIDs = new Map();
    NotificationTracker.watch("about-protocol", this);
  },

  track: function(path, register) {
    let contractID = path[1];
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    if (register) {
      let instance = new AboutProtocolInstance(contractID);
      let classID = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator)
                      .generateUUID();

      this._instances.set(contractID, instance);
      this._classIDs.set(contractID, classID);
      registrar.registerFactory(classID, this._classDescription, contractID, instance);
    } else {
      let instance = this._instances.get(contractID);
      let classID = this._classIDs.get(contractID);
      registrar.unregisterFactory(classID, instance);
      this._instances.delete(contractID);
      this._classIDs.delete(contractID);
    }
  },
};

// This code registers observers in the child whenever an add-on in
// the parent asks for notifications on the given topic.
let ObserverChild = {
  init: function() {
    NotificationTracker.watch("observer", this);
  },

  track: function(path, register) {
    let topic = path[1];
    if (register) {
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
  this.capturingHandler = (event) => this.handleEvent(true, event);
  this.nonCapturingHandler = (event) => this.handleEvent(false, event);
  NotificationTracker.watch("event", this);
}

EventTargetChild.prototype = {
  uninit: function() {
    NotificationTracker.unwatch("event", this);
  },

  track: function(path, register) {
    let eventType = path[1];
    let useCapture = path[2];
    let listener = useCapture ? this.capturingHandler : this.nonCapturingHandler;
    if (register) {
      this._childGlobal.addEventListener(eventType, listener, useCapture, true);
    } else {
      this._childGlobal.removeEventListener(eventType, listener, useCapture);
    }
  },

  handleEvent: function(capturing, event) {
    let addons = NotificationTracker.findSuffixes(["event", event.type, capturing]);
    let [prefetched, cpows] = Prefetcher.prefetch("EventTarget.handleEvent",
                                                  addons,
                                                  {Event: event,
                                                   Window: this._childGlobal.content});
    cpows.event = event;
    cpows.eventTarget = event.target;

    this._childGlobal.sendRpcMessage("Addons:Event:Run",
                                     {type: event.type,
                                      capturing: capturing,
                                      isTrusted: event.isTrusted,
                                      prefetched: prefetched},
                                     cpows);
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
  uninit: function() {
    this.clearSandboxes();
  },

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
    this.clearSandboxes();
  },

  addSandbox: function(sandbox) {
    if (this.sandboxes.length == 0) {
      this.addListener();
    }
    this.sandboxes.push(sandbox);
  },

  clearSandboxes: function() {
    if (this.sandboxes.length) {
      this.removeListener();
    }
    this.sandboxes = [];
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};

let RemoteAddonsChild = {
  _ready: false,

  makeReady: function() {
    let shims = [
      Prefetcher,
      NotificationTracker,
      ContentPolicyChild,
      AboutProtocolChild,
      ObserverChild,
    ];

    for (let shim of shims) {
      try {
        shim.init();
      } catch(e) {
        Cu.reportError(e);
      }
    }
  },

  init: function(global) {

    if (!this._ready) {
      if (!Services.cpmm.initialProcessData.remoteAddonsParentInitted){
        return null;
      }

      this.makeReady();
      this._ready = true;
    }

    global.sendAsyncMessage("Addons:RegisterGlobal", {}, {global: global});

    let sandboxChild = new SandboxChild(global);
    global.addSandbox = sandboxChild.addSandbox.bind(sandboxChild);

    // Return this so it gets rooted in the content script.
    return [new EventTargetChild(global), sandboxChild];
  },

  uninit: function(perTabShims) {
    for (let shim of perTabShims) {
      try {
        shim.uninit();
      } catch(e) {
        Cu.reportError(e);
      }
    }
  },

  get useSyncWebProgress() {
    return NotificationTracker.getCount("web-progress") > 0;
  },
};
