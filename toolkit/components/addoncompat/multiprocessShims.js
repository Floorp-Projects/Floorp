/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prefetcher",
                                  "resource://gre/modules/Prefetcher.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemoteAddonsParent",
                                  "resource://gre/modules/RemoteAddonsParent.jsm");

/**
 * This service overlays the API that the browser exposes to
 * add-ons. The overlay tries to make a multiprocess browser appear as
 * much as possible like a single process browser. An overlay can
 * replace methods, getters, and setters of arbitrary browser objects.
 *
 * Most of the actual replacement code is implemented in
 * RemoteAddonsParent. The code in this service simply decides how to
 * replace code. For a given type of object (say, an
 * nsIObserverService) the code in RemoteAddonsParent can register a
 * set of replacement methods. This set is called an
 * "interposition". The service keeps track of all the different
 * interpositions. Whenever a method is called on some part of the
 * browser API, this service gets a chance to replace it. To do so, it
 * consults its map based on the type of object. If an interposition
 * is found, the given method is looked up on it and called
 * instead. If no method (or no interposition) is found, then the
 * original target method is called as normal.
 *
 * For each method call, we need to determine the type of the target
 * object.  If the object is an old-style XPConnect wrapped native,
 * then the type is simply the interface that the method was called on
 * (Ci.nsIObserverService, say). For all other objects (WebIDL
 * objects, CPOWs, and normal JS objects), the type is determined by
 * calling getObjectTag.
 *
 * The interpositions defined in RemoteAddonsParent have three
 * properties: methods, getters, and setters. When accessing a
 * property, we first consult methods. If nothing is found, then we
 * consult getters or setters, depending on whether the access is a
 * get or a set.
 *
 * The methods in |methods| are functions that will be called whenever
 * the given method is called on the target object. They are passed
 * the same parameters as the original function except for two
 * additional ones at the beginning: the add-on ID and the original
 * target object that the method was called on. Additionally, the
 * value of |this| is set to the original target object.
 *
 * The values in |getters| and |setters| should also be
 * functions. They are called immediately when the given property is
 * accessed. The functions in |getters| take two parameters: the
 * add-on ID and the original target object. The functions in
 * |setters| take those arguments plus the value that the property is
 * being set to.
 */

function AddonInterpositionService() {
  Prefetcher.init();
  RemoteAddonsParent.init();

  // These maps keep track of the interpositions for all different
  // kinds of objects.
  this._interfaceInterpositions = RemoteAddonsParent.getInterfaceInterpositions();
  this._taggedInterpositions = RemoteAddonsParent.getTaggedInterpositions();

  let wl = [];
  for (let v in this._interfaceInterpositions) {
    let interp = this._interfaceInterpositions[v];
    wl.push(...Object.getOwnPropertyNames(interp.methods));
    wl.push(...Object.getOwnPropertyNames(interp.getters));
    wl.push(...Object.getOwnPropertyNames(interp.setters));
  }

  for (let v in this._taggedInterpositions) {
    let interp = this._taggedInterpositions[v];
    wl.push(...Object.getOwnPropertyNames(interp.methods));
    wl.push(...Object.getOwnPropertyNames(interp.getters));
    wl.push(...Object.getOwnPropertyNames(interp.setters));
  }

  let nameSet = new Set();
  wl = wl.filter(function(item) {
    if (nameSet.has(item))
      return true;

    nameSet.add(item);
    return true;
  });

  this._whitelist = wl;
}

AddonInterpositionService.prototype = {
  classID: Components.ID("{1363d5f0-d95e-11e3-9c1a-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonInterposition, Ci.nsISupportsWeakReference]),

  getWhitelist() {
    return this._whitelist;
  },

  // When the interface is not known for a method call, this code
  // determines the type of the target object.
  getObjectTag(target) {
    if (Cu.isCrossProcessWrapper(target)) {
      return Cu.getCrossProcessWrapperTag(target);
    }

    if (target instanceof Ci.nsIDOMXULElement) {
      if (target.localName == "browser" && target.isRemoteBrowser) {
        return "RemoteBrowserElement";
      }

      if (target.localName == "tabbrowser") {
        return "TabBrowserElement";
      }
    }

    if (target instanceof Ci.nsIDOMChromeWindow && target.gMultiProcessBrowser) {
      return "ChromeWindow";
    }

    if (target instanceof Ci.nsIDOMEventTarget) {
      return "EventTarget";
    }

    return "generic";
  },

  interposeProperty(addon, target, iid, prop) {
    let interp;
    if (iid) {
      interp = this._interfaceInterpositions[iid];
    } else {
      try {
        interp = this._taggedInterpositions[this.getObjectTag(target)];
      } catch (e) {
        Cu.reportError(new Components.Exception("Failed to interpose object", e.result, Components.stack.caller));
      }
    }

    if (!interp) {
      return Prefetcher.lookupInCache(addon, target, prop);
    }

    let desc = { configurable: false, enumerable: true };

    if ("methods" in interp && prop in interp.methods) {
      desc.writable = false;
      desc.value = function(...args) {
        return interp.methods[prop](addon, target, ...args);
      }

      return desc;
    } else if ("getters" in interp && prop in interp.getters) {
      desc.get = function() { return interp.getters[prop](addon, target); };

      if ("setters" in interp && prop in interp.setters) {
        desc.set = function(v) { return interp.setters[prop](addon, target, v); };
      }

      return desc;
    }

    return Prefetcher.lookupInCache(addon, target, prop);
  },

  interposeCall(addonId, originalFunc, originalThis, args) {
    args.splice(0, 0, addonId);
    return originalFunc.apply(originalThis, args);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonInterpositionService]);
