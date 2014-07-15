// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteAddonsParent"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');

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
let NotificationTracker = {
  // _paths is a multi-level dictionary. Let's add paths [A, B] and
  // [A, C]. Then _paths will look like this:
  //   { 'A': { 'B': { '_count': 1 }, 'C': { '_count': 1 } } }
  // Each component in a path will be a key in some dictionary. At the
  // end, the _count property keeps track of how many instances of the
  // given path are present in _paths.
  _paths: {},

  init: function() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:GetNotifications", this);
  },

  add: function(path) {
    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }
    let count = tracked._count || 0;
    count++;
    tracked._count = count;

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.broadcastAsyncMessage("Addons:AddNotification", path);
  },

  remove: function(path) {
    let tracked = this._paths;
    for (let component of path) {
      tracked = setDefault(tracked, component, {});
    }
    tracked._count--;

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.broadcastAsyncMessage("Addons:RemoveNotification", path);
  },

  receiveMessage: function(msg) {
    if (msg.name == "Addons:GetNotifications") {
      return this._paths;
    }
  }
};
NotificationTracker.init();

// An interposition is an object with three properties: methods,
// getters, and setters. See multiprocessShims.js for an explanation
// of how these are used. The constructor here just allows one
// interposition to inherit members from another.
function Interposition(base)
{
  if (base) {
    this.methods = Object.create(base.methods);
    this.getters = Object.create(base.getters);
    this.setters = Object.create(base.setters);
  } else {
    this.methods = {};
    this.getters = {};
    this.setters = {};
  }
}

// This object is responsible for notifying the child when a new
// content policy is added or removed. It also runs all the registered
// add-on content policies when the child asks it to do so.
let ContentPolicyParent = {
  init: function() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:ContentPolicy:Run", this);

    this._policies = [];
  },

  addContentPolicy: function(cid) {
    this._policies.push(cid);
    NotificationTracker.add(["content-policy"]);
  },

  removeContentPolicy: function(cid) {
    let index = this._policies.lastIndexOf(cid);
    if (index > -1) {
      this._policies.splice(index, 1);
    }

    NotificationTracker.remove(["content-policy"]);
  },

  receiveMessage: function (aMessage) {
    switch (aMessage.name) {
      case "Addons:ContentPolicy:Run":
        return this.shouldLoad(aMessage.data, aMessage.objects);
        break;
    }
  },

  shouldLoad: function(aData, aObjects) {
    for (let policyCID of this._policies) {
      let policy = Cc[policyCID].getService(Ci.nsIContentPolicy);
      try {
        let result = policy.shouldLoad(aObjects.contentType,
                                       aObjects.contentLocation,
                                       aObjects.requestOrigin,
                                       aObjects.node,
                                       aObjects.mimeTypeGuess,
                                       null);
        if (result != Ci.nsIContentPolicy.ACCEPT && result != 0)
          return result;
      } catch (e) {
        Cu.reportError(e);
      }
    }

    return Ci.nsIContentPolicy.ACCEPT;
  },
};
ContentPolicyParent.init();

// This interposition intercepts calls to add or remove new content
// policies and forwards these requests to ContentPolicyParent.
let CategoryManagerInterposition = new Interposition();

CategoryManagerInterposition.methods.addCategoryEntry =
  function(addon, target, category, entry, value, persist, replace) {
    if (category == "content-policy") {
      ContentPolicyParent.addContentPolicy(entry);
    }

    target.addCategoryEntry(category, entry, value, persist, replace);
  };

CategoryManagerInterposition.methods.deleteCategoryEntry =
  function(addon, target, category, entry, persist) {
    if (category == "content-policy") {
      ContentPolicyParent.remoteContentPolicy(entry);
    }

    target.deleteCategoryEntry(category, entry, persist);
  };

// This object manages add-on observers that might fire in the child
// process. Rather than managing the observers itself, it uses the
// parent's observer service. When an add-on listens on topic T,
// ObserverParent asks the child process to listen on T. It also adds
// an observer in the parent for the topic e10s-T. When the T observer
// fires in the child, the parent fires all the e10s-T observers,
// passing them CPOWs for the subject and data. We don't want to use T
// in the parent because there might be non-add-on T observers that
// won't expect to get notified in this case.
let ObserverParent = {
  init: function() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:Observer:Run", this);
  },

  addObserver: function(observer, topic, ownsWeak) {
    Services.obs.addObserver(observer, "e10s-" + topic, ownsWeak);
    NotificationTracker.add(["observer", topic]);
  },

  removeObserver: function(observer, topic) {
    Services.obs.removeObserver(observer, "e10s-" + topic);
    NotificationTracker.remove(["observer", topic]);
  },

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "Addons:Observer:Run":
        this.notify(msg.objects.subject, msg.objects.topic, msg.objects.data);
        break;
    }
  },

  notify: function(subject, topic, data) {
    let e = Services.obs.enumerateObservers("e10s-" + topic);
    while (e.hasMoreElements()) {
      let obs = e.getNext().QueryInterface(Ci.nsIObserver);
      try {
        obs.observe(subject, topic, data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }
};
ObserverParent.init();

// We only forward observers for these topics.
let TOPIC_WHITELIST = ["content-document-global-created",
                       "document-element-inserted",];

// This interposition listens for
// nsIObserverService.{add,remove}Observer.
let ObserverInterposition = new Interposition();

ObserverInterposition.methods.addObserver =
  function(addon, target, observer, topic, ownsWeak) {
    if (TOPIC_WHITELIST.indexOf(topic) >= 0) {
      ObserverParent.addObserver(observer, topic);
    }

    target.addObserver(observer, topic, ownsWeak);
  };

ObserverInterposition.methods.removeObserver =
  function(addon, target, observer, topic) {
    if (TOPIC_WHITELIST.indexOf(topic) >= 0) {
      ObserverParent.removeObserver(observer, topic);
    }

    target.removeObserver(observer, topic);
  };

// This object is responsible for forwarding events from the child to
// the parent.
let EventTargetParent = {
  init: function() {
    // The _listeners map goes from targets (either <browser> elements
    // or windows) to a dictionary from event types to listeners.
    this._listeners = new WeakMap();

    let mm = Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("Addons:Event:Run", this);
  },

  // If target is not on the path from a <browser> element to the
  // window root, then we return null here to ignore the
  // target. Otherwise, if the target is a browser-specific element
  // (the <browser> or <tab> elements), then we return the
  // <browser>. If it's some generic element, then we return the
  // window itself.
  redirectEventTarget: function(target) {
    if (Cu.isCrossProcessWrapper(target)) {
      return null;
    }

    if (target instanceof Ci.nsIDOMChromeWindow) {
      return target;
    }

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    if (target instanceof Ci.nsIDOMXULElement) {
      if (target.localName == "browser") {
        return target;
      } else if (target.localName == "tab") {
        return target.linkedBrowser;
      }

      // Check if |target| is somewhere on the patch from the
      // <tabbrowser> up to the root element.
      let window = target.ownerDocument.defaultView;
      if (target.contains(window.gBrowser)) {
        return window;
      }
    }

    return null;
  },

  // When a given event fires in the child, we fire it on the
  // <browser> element and the window since those are the two possible
  // results of redirectEventTarget.
  getTargets: function(browser) {
    let window = browser.ownerDocument.defaultView;
    return [browser, window];
  },

  addEventListener: function(target, type, listener, useCapture, wantsUntrusted) {
    let newTarget = this.redirectEventTarget(target);
    if (!newTarget) {
      return;
    }

    useCapture = useCapture || false;
    wantsUntrusted = wantsUntrusted || false;

    NotificationTracker.add(["event", type, useCapture]);

    let listeners = this._listeners.get(newTarget);
    if (!listeners) {
      listeners = {};
      this._listeners.set(newTarget, listeners);
    }
    let forType = setDefault(listeners, type, []);

    // If there's already an identical listener, don't do anything.
    for (let i = 0; i < forType.length; i++) {
      if (forType[i].listener === listener &&
          forType[i].useCapture === useCapture &&
          forType[i].wantsUntrusted === wantsUntrusted) {
        return;
      }
    }

    forType.push({listener: listener, wantsUntrusted: wantsUntrusted, useCapture: useCapture});
  },

  removeEventListener: function(target, type, listener, useCapture) {
    let newTarget = this.redirectEventTarget(target);
    if (!newTarget) {
      return;
    }

    useCapture = useCapture || false;

    let listeners = this._listeners.get(newTarget);
    if (!listeners) {
      return;
    }
    let forType = setDefault(listeners, type, []);

    for (let i = 0; i < forType.length; i++) {
      if (forType[i].listener === listener && forType[i].useCapture === useCapture) {
        forType.splice(i, 1);
        NotificationTracker.remove(["event", type, useCapture]);
        break;
      }
    }
  },

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "Addons:Event:Run":
        this.dispatch(msg.target, msg.data.type, msg.data.isTrusted, msg.objects.event);
        break;
    }
  },

  dispatch: function(browser, type, isTrusted, event) {
    let targets = this.getTargets(browser);
    for (target of targets) {
      let listeners = this._listeners.get(target);
      if (!listeners) {
        continue;
      }
      let forType = setDefault(listeners, type, []);
      for (let {listener, wantsUntrusted} of forType) {
        if (wantsUntrusted || isTrusted) {
          try {
            if ("handleEvent" in listener) {
              listener.handleEvent(event);
            } else {
              listener.call(event.target, event);
            }
          } catch (e) {
            Cu.reportError(e);
          }
        }
      }
    }
  }
};
EventTargetParent.init();

// This interposition redirects addEventListener and
// removeEventListener to EventTargetParent.
let EventTargetInterposition = new Interposition();

EventTargetInterposition.methods.addEventListener =
  function(addon, target, type, listener, useCapture, wantsUntrusted) {
    EventTargetParent.addEventListener(target, type, listener, useCapture, wantsUntrusted);
    target.addEventListener(type, listener, useCapture, wantsUntrusted);
  };

EventTargetInterposition.methods.removeEventListener =
  function(addon, target, type, listener, useCapture) {
    EventTargetParent.removeEventListener(target, type, listener, useCapture);
    target.removeEventListener(type, listener, useCapture);
  };

let RemoteAddonsParent = {
  init: function() {
  },

  getInterfaceInterpositions: function() {
    let result = {};

    function register(intf, interp) {
      result[intf.number] = interp;
    }

    register(Ci.nsICategoryManager, CategoryManagerInterposition);
    register(Ci.nsIObserverService, ObserverInterposition);

    return result;
  },

  getTaggedInterpositions: function() {
    let result = {};

    function register(tag, interp) {
      result[tag] = interp;
    }

    register("EventTarget", EventTargetInterposition);

    return result;
  },
};
