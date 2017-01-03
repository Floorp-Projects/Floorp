// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteAddonsParent"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/RemoteWebProgress.jsm");
Cu.import('resource://gre/modules/Services.jsm');

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Prefetcher",
                                  "resource://gre/modules/Prefetcher.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CompatWarning",
                                  "resource://gre/modules/CompatWarning.jsm");

Cu.permitCPOWsInScope(this);

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
function Interposition(name, base)
{
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

// This object is responsible for notifying the child when a new
// content policy is added or removed. It also runs all the registered
// add-on content policies when the child asks it to do so.
var ContentPolicyParent = {
  init() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:ContentPolicy:Run", this);

    this._policies = new Map();
  },

  addContentPolicy(addon, name, cid) {
    this._policies.set(name, cid);
    NotificationTracker.add(["content-policy", addon]);
  },

  removeContentPolicy(addon, name) {
    this._policies.delete(name);
    NotificationTracker.remove(["content-policy", addon]);
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Addons:ContentPolicy:Run":
        return this.shouldLoad(aMessage.data, aMessage.objects);
    }
    return undefined;
  },

  shouldLoad(aData, aObjects) {
    for (let policyCID of this._policies.values()) {
      let policy;
      try {
        policy = Cc[policyCID].getService(Ci.nsIContentPolicy);
      } catch (e) {
        // Current Gecko behavior is to ignore entries that don't QI.
        continue;
      }
      try {
        let contentLocation = BrowserUtils.makeURI(aData.contentLocation);
        let requestOrigin = aData.requestOrigin ? BrowserUtils.makeURI(aData.requestOrigin) : null;

        let result = Prefetcher.withPrefetching(aData.prefetched, aObjects, () => {
          return policy.shouldLoad(aData.contentType,
                                   contentLocation,
                                   requestOrigin,
                                   aObjects.node,
                                   aData.mimeTypeGuess,
                                   null,
                                   aData.requestPrincipal);
        });
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
var CategoryManagerInterposition = new Interposition("CategoryManagerInterposition");

CategoryManagerInterposition.methods.addCategoryEntry =
  function(addon, target, category, entry, value, persist, replace) {
    if (category == "content-policy") {
      CompatWarning.warn("content-policy should be added from the child process only.",
                         addon, CompatWarning.warnings.nsIContentPolicy);
      ContentPolicyParent.addContentPolicy(addon, entry, value);
    }

    target.addCategoryEntry(category, entry, value, persist, replace);
  };

CategoryManagerInterposition.methods.deleteCategoryEntry =
  function(addon, target, category, entry, persist) {
    if (category == "content-policy") {
      CompatWarning.warn("content-policy should be removed from the child process only.",
                         addon, CompatWarning.warnings.nsIContentPolicy);
      ContentPolicyParent.removeContentPolicy(addon, entry);
    }

    target.deleteCategoryEntry(category, entry, persist);
  };

// This shim handles the case where an add-on registers an about:
// protocol handler in the parent and we want the child to be able to
// use it. This code is pretty specific to Adblock's usage.
var AboutProtocolParent = {
  init() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:AboutProtocol:GetURIFlags", this);
    ppmm.addMessageListener("Addons:AboutProtocol:OpenChannel", this);
    this._protocols = [];
  },

  registerFactory(addon, class_, className, contractID, factory) {
    this._protocols.push({contractID, factory});
    NotificationTracker.add(["about-protocol", contractID, addon]);
  },

  unregisterFactory(addon, class_, factory) {
    for (let i = 0; i < this._protocols.length; i++) {
      if (this._protocols[i].factory == factory) {
        NotificationTracker.remove(["about-protocol", this._protocols[i].contractID, addon]);
        this._protocols.splice(i, 1);
        break;
      }
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Addons:AboutProtocol:GetURIFlags":
        return this.getURIFlags(msg);
      case "Addons:AboutProtocol:OpenChannel":
        return this.openChannel(msg);
    }
    return undefined;
  },

  getURIFlags(msg) {
    let uri = BrowserUtils.makeURI(msg.data.uri);
    let contractID = msg.data.contractID;
    let module = Cc[contractID].getService(Ci.nsIAboutModule);
    try {
      return module.getURIFlags(uri);
    } catch (e) {
      Cu.reportError(e);
      return undefined;
    }
  },

  // We immediately read all the data out of the channel here and
  // return it to the child.
  openChannel(msg) {
    function wrapGetInterface(cpow) {
      return {
        getInterface(intf) { return cpow.getInterface(intf); }
      };
    }

    let uri = BrowserUtils.makeURI(msg.data.uri);
    let channelParams;
    if (msg.data.contentPolicyType === Ci.nsIContentPolicy.TYPE_DOCUMENT) {
      // For TYPE_DOCUMENT loads, we cannot recreate the loadinfo here in the
      // parent. In that case, treat this as a chrome (addon)-requested
      // subload. When we use the data in the child, we'll load it into the
      // correctly-principaled document.
      channelParams = {
        uri,
        contractID: msg.data.contractID,
        loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
        securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
      };
    } else {
      // We can recreate the loadinfo here in the parent for non TYPE_DOCUMENT
      // loads.
      channelParams = {
        uri,
        contractID: msg.data.contractID,
        loadingPrincipal: msg.data.loadingPrincipal,
        securityFlags: msg.data.securityFlags,
        contentPolicyType: msg.data.contentPolicyType
      };
    }

    try {
      let channel = NetUtil.newChannel(channelParams);

      // We're not allowed to set channel.notificationCallbacks to a
      // CPOW, since the setter for notificationCallbacks is in C++,
      // which can't tolerate CPOWs. Instead we just use a JS object
      // that wraps the CPOW.
      channel.notificationCallbacks = wrapGetInterface(msg.objects.notificationCallbacks);
      if (msg.objects.loadGroupNotificationCallbacks) {
        channel.loadGroup = {notificationCallbacks: msg.objects.loadGroupNotificationCallbacks};
      } else {
        channel.loadGroup = null;
      }
      let stream = channel.open2();
      let data = NetUtil.readInputStreamToString(stream, stream.available(), {});
      return {
        data,
        contentType: channel.contentType
      };
    } catch (e) {
      Cu.reportError(e);
      return undefined;
    }
  },
};
AboutProtocolParent.init();

var ComponentRegistrarInterposition = new Interposition("ComponentRegistrarInterposition");

ComponentRegistrarInterposition.methods.registerFactory =
  function(addon, target, class_, className, contractID, factory) {
    if (contractID && contractID.startsWith("@mozilla.org/network/protocol/about;1?")) {
      CompatWarning.warn("nsIAboutModule should be registered in the content process" +
                         " as well as the chrome process. (If you do that already, ignore" +
                         " this warning.)",
                         addon, CompatWarning.warnings.nsIAboutModule);
      AboutProtocolParent.registerFactory(addon, class_, className, contractID, factory);
    }

    target.registerFactory(class_, className, contractID, factory);
  };

ComponentRegistrarInterposition.methods.unregisterFactory =
  function(addon, target, class_, factory) {
    AboutProtocolParent.unregisterFactory(addon, class_, factory);
    target.unregisterFactory(class_, factory);
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
var ObserverParent = {
  init() {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("Addons:Observer:Run", this);
  },

  addObserver(addon, observer, topic, ownsWeak) {
    Services.obs.addObserver(observer, "e10s-" + topic, ownsWeak);
    NotificationTracker.add(["observer", topic, addon]);
  },

  removeObserver(addon, observer, topic) {
    Services.obs.removeObserver(observer, "e10s-" + topic);
    NotificationTracker.remove(["observer", topic, addon]);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Addons:Observer:Run":
        this.notify(msg.objects.subject, msg.objects.topic, msg.objects.data);
        break;
    }
  },

  notify(subject, topic, data) {
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
var TOPIC_WHITELIST = [
  "content-document-global-created",
  "document-element-inserted",
  "dom-window-destroyed",
  "inner-window-destroyed",
  "outer-window-destroyed",
  "csp-on-violate-policy",
];

// This interposition listens for
// nsIObserverService.{add,remove}Observer.
var ObserverInterposition = new Interposition("ObserverInterposition");

ObserverInterposition.methods.addObserver =
  function(addon, target, observer, topic, ownsWeak) {
    if (TOPIC_WHITELIST.indexOf(topic) >= 0) {
      CompatWarning.warn(`${topic} observer should be added from the child process only.`,
                         addon, CompatWarning.warnings.observers);

      ObserverParent.addObserver(addon, observer, topic);
    }

    target.addObserver(observer, topic, ownsWeak);
  };

ObserverInterposition.methods.removeObserver =
  function(addon, target, observer, topic) {
    if (TOPIC_WHITELIST.indexOf(topic) >= 0) {
      ObserverParent.removeObserver(addon, observer, topic);
    }

    target.removeObserver(observer, topic);
  };

// This object is responsible for forwarding events from the child to
// the parent.
var EventTargetParent = {
  init() {
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
  redirectEventTarget(target) {
    if (Cu.isCrossProcessWrapper(target)) {
      return null;
    }

    if (target instanceof Ci.nsIDOMChromeWindow) {
      return target;
    }

    if (target instanceof Ci.nsIDOMXULElement) {
      if (target.localName == "browser") {
        return target;
      } else if (target.localName == "tab") {
        return target.linkedBrowser;
      }

      // Check if |target| is somewhere on the patch from the
      // <tabbrowser> up to the root element.
      let window = target.ownerDocument.defaultView;
      if (window && target.contains(window.gBrowser)) {
        return window;
      }
    }

    return null;
  },

  // When a given event fires in the child, we fire it on the
  // <browser> element and the window since those are the two possible
  // results of redirectEventTarget.
  getTargets(browser) {
    let window = browser.ownerDocument.defaultView;
    return [browser, window];
  },

  addEventListener(addon, target, type, listener, useCapture, wantsUntrusted, delayedWarning) {
    let newTarget = this.redirectEventTarget(target);
    if (!newTarget) {
      return;
    }

    useCapture = useCapture || false;
    wantsUntrusted = wantsUntrusted || false;

    NotificationTracker.add(["event", type, useCapture, addon]);

    let listeners = this._listeners.get(newTarget);
    if (!listeners) {
      listeners = {};
      this._listeners.set(newTarget, listeners);
    }
    let forType = setDefault(listeners, type, []);

    // If there's already an identical listener, don't do anything.
    for (let i = 0; i < forType.length; i++) {
      if (forType[i].listener === listener &&
          forType[i].target === target &&
          forType[i].useCapture === useCapture &&
          forType[i].wantsUntrusted === wantsUntrusted) {
        return;
      }
    }

    forType.push({listener,
                  target,
                  wantsUntrusted,
                  useCapture,
                  delayedWarning});
  },

  removeEventListener(addon, target, type, listener, useCapture) {
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
      if (forType[i].listener === listener &&
          forType[i].target === target &&
          forType[i].useCapture === useCapture) {
        forType.splice(i, 1);
        NotificationTracker.remove(["event", type, useCapture, addon]);
        break;
      }
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Addons:Event:Run":
        this.dispatch(msg.target, msg.data.type, msg.data.capturing,
                      msg.data.isTrusted, msg.data.prefetched, msg.objects);
        break;
    }
  },

  dispatch(browser, type, capturing, isTrusted, prefetched, cpows) {
    let event = cpows.event;
    let eventTarget = cpows.eventTarget;
    let targets = this.getTargets(browser);
    for (let target of targets) {
      let listeners = this._listeners.get(target);
      if (!listeners) {
        continue;
      }
      let forType = setDefault(listeners, type, []);

      // Make a copy in case they call removeEventListener in the listener.
      let handlers = [];
      for (let {listener, target, wantsUntrusted, useCapture, delayedWarning} of forType) {
        if ((wantsUntrusted || isTrusted) && useCapture == capturing) {
          // Issue a warning for this listener.
          delayedWarning();

          handlers.push([listener, target]);
        }
      }

      for (let [handler, target] of handlers) {
        let EventProxy = {
          get(knownProps, name) {
            if (knownProps.hasOwnProperty(name))
              return knownProps[name];
            return event[name];
          }
        }
        let proxyEvent = new Proxy({
          currentTarget: target,
          target: eventTarget,
          type,
          QueryInterface(iid) {
            if (iid.equals(Ci.nsISupports) ||
                iid.equals(Ci.nsIDOMEventTarget))
              return proxyEvent;
            // If event deson't support the interface this will throw. If it
            // does we want to return the proxy
            event.QueryInterface(iid);
            return proxyEvent;
          }
        }, EventProxy);

        try {
          Prefetcher.withPrefetching(prefetched, cpows, () => {
            if ("handleEvent" in handler) {
              handler.handleEvent(proxyEvent);
            } else {
              handler.call(eventTarget, proxyEvent);
            }
          });
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }
  }
};
EventTargetParent.init();

// This function returns a listener that will not fire on events where
// the target is a remote xul:browser element itself. We'd rather let
// the child process handle the event and pass it up via
// EventTargetParent.
var filteringListeners = new WeakMap();
function makeFilteringListener(eventType, listener)
{
  // Some events are actually targeted at the <browser> element
  // itself, so we only handle the ones where know that won't happen.
  let eventTypes = ["mousedown", "mouseup", "click"];
  if (!eventTypes.includes(eventType) || !listener ||
      (typeof listener != "object" && typeof listener != "function")) {
    return listener;
  }

  if (filteringListeners.has(listener)) {
    return filteringListeners.get(listener);
  }

  function filter(event) {
    let target = event.originalTarget;
    if (target instanceof Ci.nsIDOMXULElement &&
        target.localName == "browser" &&
        target.isRemoteBrowser) {
      return;
    }

    if ("handleEvent" in listener) {
      listener.handleEvent(event);
    } else {
      listener.call(event.target, event);
    }
  }
  filteringListeners.set(listener, filter);
  return filter;
}

// This interposition redirects addEventListener and
// removeEventListener to EventTargetParent.
var EventTargetInterposition = new Interposition("EventTargetInterposition");

EventTargetInterposition.methods.addEventListener =
  function(addon, target, type, listener, useCapture, wantsUntrusted) {
    let delayed = CompatWarning.delayedWarning(
      `Registering a ${type} event listener on content DOM nodes` +
        " needs to happen in the content process.",
      addon, CompatWarning.warnings.DOM_events);

    EventTargetParent.addEventListener(addon, target, type, listener, useCapture, wantsUntrusted, delayed);
    target.addEventListener(type, makeFilteringListener(type, listener), useCapture, wantsUntrusted);
  };

EventTargetInterposition.methods.removeEventListener =
  function(addon, target, type, listener, useCapture) {
    EventTargetParent.removeEventListener(addon, target, type, listener, useCapture);
    target.removeEventListener(type, makeFilteringListener(type, listener), useCapture);
  };

// This interposition intercepts accesses to |rootTreeItem| on a child
// process docshell. In the child, each docshell is its own
// root. However, add-ons expect the root to be the chrome docshell,
// so we make that happen here.
var ContentDocShellTreeItemInterposition = new Interposition("ContentDocShellTreeItemInterposition");

ContentDocShellTreeItemInterposition.getters.rootTreeItem =
  function(addon, target) {
    // The chrome global in the child.
    let chromeGlobal = target.rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIContentFrameMessageManager);

    // Map it to a <browser> element and window.
    let browser = RemoteAddonsParent.globalToBrowser.get(chromeGlobal);
    if (!browser) {
      // Somehow we have a CPOW from the child, but it hasn't sent us
      // its global yet. That shouldn't happen, but return null just
      // in case.
      return null;
    }

    let chromeWin = browser.ownerDocument.defaultView;

    // Return that window's docshell.
    return chromeWin.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem);
  };

function chromeGlobalForContentWindow(window)
{
    return window
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIContentFrameMessageManager);
}

// This object manages sandboxes created with content principals in
// the parent. We actually create these sandboxes in the child process
// so that the code loaded into them runs there. The resulting sandbox
// object is a CPOW. This is primarly useful for Greasemonkey.
var SandboxParent = {
  componentsMap: new WeakMap(),

  makeContentSandbox(addon, chromeGlobal, principals, ...rest) {
    CompatWarning.warn("This sandbox should be created from the child process.",
                       addon, CompatWarning.warnings.sandboxes);
    if (rest.length) {
      // Do a shallow copy of the options object into the child
      // process. This way we don't have to access it through a Chrome
      // object wrapper, which would require __exposedProps__.
      //
      // The only object property here is sandboxPrototype. We assume
      // it's a child process object (since that's what Greasemonkey
      // does) and leave it alone.
      let options = rest[0];
      let optionsCopy = new chromeGlobal.Object();
      for (let prop in options) {
        optionsCopy[prop] = options[prop];
      }
      rest[0] = optionsCopy;
    }

    // Make a sandbox in the child.
    let cu = chromeGlobal.Components.utils;
    let sandbox = cu.Sandbox(principals, ...rest);

    // We need to save the sandbox in the child so it won't get
    // GCed. The child will drop this reference at the next
    // navigation.
    chromeGlobal.addSandbox(sandbox);

    // The sandbox CPOW will be kept alive by whomever we return it
    // to. Its lifetime is unrelated to that of the sandbox object in
    // the child.
    this.componentsMap.set(sandbox, cu);
    return sandbox;
  },

  evalInSandbox(code, sandbox, ...rest) {
    let cu = this.componentsMap.get(sandbox);
    return cu.evalInSandbox(code, sandbox, ...rest);
  }
};

// This interposition redirects calls to Cu.Sandbox and
// Cu.evalInSandbox to SandboxParent if the principals are content
// principals.
var ComponentsUtilsInterposition = new Interposition("ComponentsUtilsInterposition");

ComponentsUtilsInterposition.methods.Sandbox =
  function(addon, target, principals, ...rest) {
    // principals can be a window object, a list of window objects, or
    // something else (a string, for example).
    if (principals &&
        typeof(principals) == "object" &&
        Cu.isCrossProcessWrapper(principals) &&
        principals instanceof Ci.nsIDOMWindow) {
      let chromeGlobal = chromeGlobalForContentWindow(principals);
      return SandboxParent.makeContentSandbox(addon, chromeGlobal, principals, ...rest);
    } else if (principals &&
               typeof(principals) == "object" &&
               "every" in principals &&
               principals.length &&
               principals.every(e => e instanceof Ci.nsIDOMWindow && Cu.isCrossProcessWrapper(e))) {
      let chromeGlobal = chromeGlobalForContentWindow(principals[0]);

      // The principals we pass to the content process must use an
      // Array object from the content process.
      let array = new chromeGlobal.Array();
      for (let i = 0; i < principals.length; i++) {
        array[i] = principals[i];
      }
      return SandboxParent.makeContentSandbox(addon, chromeGlobal, array, ...rest);
    }
    return Components.utils.Sandbox(principals, ...rest);
  };

ComponentsUtilsInterposition.methods.evalInSandbox =
  function(addon, target, code, sandbox, ...rest) {
    if (sandbox && Cu.isCrossProcessWrapper(sandbox)) {
      return SandboxParent.evalInSandbox(code, sandbox, ...rest);
    }
    return Components.utils.evalInSandbox(code, sandbox, ...rest);
  };

// This interposition handles cases where an add-on tries to import a
// chrome XUL node into a content document. It doesn't actually do the
// import, which we can't support. It just avoids throwing an
// exception.
var ContentDocumentInterposition = new Interposition("ContentDocumentInterposition");

ContentDocumentInterposition.methods.importNode =
  function(addon, target, node, deep) {
    if (!Cu.isCrossProcessWrapper(node)) {
      // Trying to import a node from the parent process into the
      // child process. We don't support this now. Video Download
      // Helper does this in domhook-service.js to add a XUL
      // popupmenu to content.
      Cu.reportError("Calling contentDocument.importNode on a XUL node is not allowed.");
      return node;
    }

    return target.importNode(node, deep);
  };

// This interposition ensures that calling browser.docShell from an
// add-on returns a CPOW around the dochell.
var RemoteBrowserElementInterposition = new Interposition("RemoteBrowserElementInterposition",
                                                          EventTargetInterposition);

RemoteBrowserElementInterposition.getters.docShell = function(addon, target) {
  CompatWarning.warn("Direct access to content docshell will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);
  let remoteChromeGlobal = RemoteAddonsParent.browserToGlobal.get(target);
  if (!remoteChromeGlobal) {
    // We may not have any messages from this tab yet.
    return null;
  }
  return remoteChromeGlobal.docShell;
};

RemoteBrowserElementInterposition.getters.sessionHistory = function(addon, target) {
  CompatWarning.warn("Direct access to browser.sessionHistory will no longer " +
                     "work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  return getSessionHistory(target);
}

// We use this in place of the real browser.contentWindow if we
// haven't yet received a CPOW for the child process's window. This
// happens if the tab has just started loading.
function makeDummyContentWindow(browser) {
  let dummyContentWindow = {
    set location(url) {
      browser.loadURI(url, null, null);
    },
    document: {
      readyState: "loading",
      location: { href: "about:blank" }
    },
    frames: [],
  };
  dummyContentWindow.top = dummyContentWindow;
  dummyContentWindow.document.defaultView = dummyContentWindow;
  browser._contentWindow = dummyContentWindow;
  return dummyContentWindow;
}

RemoteBrowserElementInterposition.getters.contentWindow = function(addon, target) {
  CompatWarning.warn("Direct access to browser.contentWindow will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  // If we don't have a CPOW yet, just return something we can use for
  // setting the location. This is useful for tests that create a tab
  // and immediately set contentWindow.location.
  if (!target.contentWindowAsCPOW) {
    CompatWarning.warn("CPOW to the content window does not exist yet, dummy content window is created.");
    return makeDummyContentWindow(target);
  }
  return target.contentWindowAsCPOW;
};

function getContentDocument(addon, browser)
{
  if (!browser.contentWindowAsCPOW) {
    return makeDummyContentWindow(browser).document;
  }

  let doc = Prefetcher.lookupInCache(addon, browser.contentWindowAsCPOW, "document");
  if (doc) {
    return doc;
  }

  return browser.contentWindowAsCPOW.document;
}

function getSessionHistory(browser) {
  let remoteChromeGlobal = RemoteAddonsParent.browserToGlobal.get(browser);
  if (!remoteChromeGlobal) {
    CompatWarning.warn("CPOW for the remote browser docShell hasn't been received yet.");
    // We may not have any messages from this tab yet.
    return null;
  }
  return remoteChromeGlobal.docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
}

RemoteBrowserElementInterposition.getters.contentDocument = function(addon, target) {
  CompatWarning.warn("Direct access to browser.contentDocument will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  return getContentDocument(addon, target);
};

var TabBrowserElementInterposition = new Interposition("TabBrowserElementInterposition",
                                                       EventTargetInterposition);

TabBrowserElementInterposition.getters.contentWindow = function(addon, target) {
  CompatWarning.warn("Direct access to gBrowser.contentWindow will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  if (!target.selectedBrowser.contentWindowAsCPOW) {
    return makeDummyContentWindow(target.selectedBrowser);
  }
  return target.selectedBrowser.contentWindowAsCPOW;
};

TabBrowserElementInterposition.getters.contentDocument = function(addon, target) {
  CompatWarning.warn("Direct access to gBrowser.contentDocument will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  let browser = target.selectedBrowser;
  return getContentDocument(addon, browser);
};

TabBrowserElementInterposition.getters.sessionHistory = function(addon, target) {
  CompatWarning.warn("Direct access to gBrowser.sessionHistory will no " +
                     "longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);
  let browser = target.selectedBrowser;
  if (!browser.isRemoteBrowser) {
    return browser.sessionHistory;
  }
  return getSessionHistory(browser);
};

// This function returns a wrapper around an
// nsIWebProgressListener. When the wrapper is invoked, it calls the
// real listener but passes CPOWs for the nsIWebProgress and
// nsIRequest arguments.
var progressListeners = {global: new WeakMap(), tabs: new WeakMap()};
function wrapProgressListener(kind, listener)
{
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
  if (!target.ownerDocument.defaultView.gMultiProcessBrowser) {
    return target.addProgressListener(listener);
  }

  NotificationTracker.add(["web-progress", addon]);
  return target.addProgressListener(wrapProgressListener("global", listener));
};

TabBrowserElementInterposition.methods.removeProgressListener = function(addon, target, listener) {
  if (!target.ownerDocument.defaultView.gMultiProcessBrowser) {
    return target.removeProgressListener(listener);
  }

  NotificationTracker.remove(["web-progress", addon]);
  return target.removeProgressListener(wrapProgressListener("global", listener));
};

TabBrowserElementInterposition.methods.addTabsProgressListener = function(addon, target, listener) {
  if (!target.ownerDocument.defaultView.gMultiProcessBrowser) {
    return target.addTabsProgressListener(listener);
  }

  NotificationTracker.add(["web-progress", addon]);
  return target.addTabsProgressListener(wrapProgressListener("tabs", listener));
};

TabBrowserElementInterposition.methods.removeTabsProgressListener = function(addon, target, listener) {
  if (!target.ownerDocument.defaultView.gMultiProcessBrowser) {
    return target.removeTabsProgressListener(listener);
  }

  NotificationTracker.remove(["web-progress", addon]);
  return target.removeTabsProgressListener(wrapProgressListener("tabs", listener));
};

var ChromeWindowInterposition = new Interposition("ChromeWindowInterposition",
                                                  EventTargetInterposition);

// _content is for older add-ons like pinboard and all-in-one gestures
// that should be using content instead.
ChromeWindowInterposition.getters.content =
ChromeWindowInterposition.getters._content = function(addon, target) {
  CompatWarning.warn("Direct access to chromeWindow.content will no longer work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  let browser = target.gBrowser.selectedBrowser;
  if (!browser.contentWindowAsCPOW) {
    return makeDummyContentWindow(browser);
  }
  return browser.contentWindowAsCPOW;
};

var RemoteWebNavigationInterposition = new Interposition("RemoteWebNavigation");

RemoteWebNavigationInterposition.getters.sessionHistory = function(addon, target) {
  CompatWarning.warn("Direct access to webNavigation.sessionHistory will no longer " +
                     "work in the chrome process.",
                     addon, CompatWarning.warnings.content);

  if (target instanceof Ci.nsIDocShell) {
    // We must have a non-remote browser, so we can go ahead
    // and just return the real sessionHistory.
    return target.sessionHistory;
  }

  let impl = target.wrappedJSObject;
  if (!impl) {
    return null;
  }

  let browser = impl._browser;

  return getSessionHistory(browser);
}

var RemoteAddonsParent = {
  init() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("Addons:RegisterGlobal", this);

    Services.ppmm.initialProcessData.remoteAddonsParentInitted = true;

    this.globalToBrowser = new WeakMap();
    this.browserToGlobal = new WeakMap();
  },

  getInterfaceInterpositions() {
    let result = {};

    function register(intf, interp) {
      result[intf.number] = interp;
    }

    register(Ci.nsICategoryManager, CategoryManagerInterposition);
    register(Ci.nsIComponentRegistrar, ComponentRegistrarInterposition);
    register(Ci.nsIObserverService, ObserverInterposition);
    register(Ci.nsIXPCComponents_Utils, ComponentsUtilsInterposition);
    register(Ci.nsIWebNavigation, RemoteWebNavigationInterposition);

    return result;
  },

  getTaggedInterpositions() {
    let result = {};

    function register(tag, interp) {
      result[tag] = interp;
    }

    register("EventTarget", EventTargetInterposition);
    register("ContentDocShellTreeItem", ContentDocShellTreeItemInterposition);
    register("ContentDocument", ContentDocumentInterposition);
    register("RemoteBrowserElement", RemoteBrowserElementInterposition);
    register("TabBrowserElement", TabBrowserElementInterposition);
    register("ChromeWindow", ChromeWindowInterposition);

    return result;
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
