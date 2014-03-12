/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");
const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const {LongStringActor, ShortLongString} = require("devtools/server/actors/string");
Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

exports.register = function(handle) {
  handle.addTabActor(StorageActor, "storageActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(StorageActor);
};

// Maximum number of cookies/local storage key-value-pairs that can be sent
// over the wire to the client in one request.
const MAX_STORE_OBJECT_COUNT = 30;
// Interval for the batch job that sends the accumilated update packets to the
// client.
const UPDATE_INTERVAL = 500; // ms

// Holder for all the registered storage actors.
let storageTypePool = new Map();

/**
 * Gets an accumulated list of all storage actors registered to be used to
 * create a RetVal to define the return type of StorageActor.listStores method.
 */
function getRegisteredTypes() {
  let registeredTypes = {};
  for (let store of storageTypePool.keys()) {
    registeredTypes[store] = store;
  }
  return registeredTypes;
}

// Cookies store object
types.addDictType("cookieobject", {
  name: "string",
  value: "longstring",
  path: "nullable:string",
  host: "string",
  isDomain: "boolean",
  isSecure: "boolean",
  isHttpOnly: "boolean",
  creationTime: "number",
  lastAccessed: "number",
  expires: "number"
});

// Array of cookie store objects
types.addDictType("cookiestoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:cookieobject"
});

// Local Storage / Session Storage store object
types.addDictType("storageobject", {
  name: "string",
  value: "longstring"
});

// Array of Local Storage / Session Storage store objects
types.addDictType("storagestoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:storageobject"
});

// Update notification object
types.addDictType("storeUpdateObject", {
  changed: "nullable:json",
  deleted: "nullable:json",
  added: "nullable:json"
});

// Helper methods to create a storage actor.
let StorageActors = {};

/**
 * Creates a default object with the common methods required by all storage
 * actors.
 *
 * This default object is missing a couple of required methods that should be
 * implemented seperately for each actor. They are namely:
 *   - observe : Method which gets triggered on the notificaiton of the watched
 *               topic.
 *   - getNamesForHost : Given a host, get list of all known store names.
 *   - getValuesForHost : Given a host (and optianally a name) get all known
 *                        store objects.
 *   - toStoreObject : Given a store object, convert it to the required format
 *                     so that it can be transferred over wire.
 *   - populateStoresForHost : Given a host, populate the map of all store
 *                             objects for it
 *
 * @param {string} typeName
 *        The typeName of the actor.
 * @param {string} observationTopic
 *        The topic which this actor listens to via Notification Observers.
 * @param {string} storeObjectType
 *        The RetVal type of the store object of this actor.
 */
StorageActors.defaults = function(typeName, observationTopic, storeObjectType) {
  return {
    typeName: typeName,

    get conn() {
      return this.storageActor.conn;
    },

    /**
     * Returns a list of currently knwon hosts for the target window. This list
     * contains unique hosts from the window + all inner windows.
     */
    get hosts() {
      let hosts = new Set();
      for (let {location} of this.storageActor.windows) {
        hosts.add(this.getHostName(location));
      }
      return hosts;
    },

    /**
     * Returns all the windows present on the page. Includes main window + inner
     * iframe windows.
     */
    get windows() {
      return this.storageActor.windows;
    },

    /**
     * Converts the window.location object into host.
     */
    getHostName: function(location) {
      return location.hostname || location.href;
    },

    initialize: function(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.populateStoresForHosts();
      Services.obs.addObserver(this, observationTopic, false);
      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      events.on(this.storageActor, "window-ready", this.onWindowReady);
      events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
    },

    destroy: function() {
      this.hostVsStores = null;
      Services.obs.removeObserver(this, observationTopic, false);
      events.off(this.storageActor, "window-ready", this.onWindowReady);
      events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);
    },

    /**
     * When a new window is added to the page. This generally means that a new
     * iframe is created, or the current window is completely reloaded.
     *
     * @param {window} window
     *        The window which was added.
     */
    onWindowReady: function(window) {
      let host = this.getHostName(window.location);
      if (!this.hostVsStores.has(host)) {
        this.populateStoresForHost(host, window);
        let data = {};
        data[host] = this.getNamesForHost(host);
        this.storageActor.update("added", typeName, data);
      }
    },

    /**
     * When a window is removed from the page. This generally means that an
     * iframe was removed, or the current window reload is triggered.
     *
     * @param {window} window
     *        The window which was removed.
     */
    onWindowDestroyed: function(window) {
      let host = this.getHostName(window.location);
      if (!this.hosts.has(host)) {
        this.hostVsStores.delete(host);
        let data = {};
        data[host] = [];
        this.storageActor.update("deleted", typeName, data);
      }
    },

    form: function(form, detail) {
      if (detail === "actorid") {
        return this.actorID;
      }

      let hosts = {};
      for (let host of this.hosts) {
        hosts[host] = [];
      }

      return {
        actor: this.actorID,
        hosts: hosts
      };
    },

    /**
     * Populates a map of known hosts vs a map of stores vs value.
     */
    populateStoresForHosts: function() {
      this.hostVsStores = new Map();
      for (let host of this.hosts) {
        this.populateStoresForHost(host);
      }
    },

    /**
     * Returns a list of requested store objects. Maximum values returned are
     * MAX_STORE_OBJECT_COUNT. This method returns paginated values whose
     * starting index and total size can be controlled via the options object
     *
     * @param {string} host
     *        The host name for which the store values are required.
     * @param {array:string} names
     *        Array containing the names of required store objects. Empty if all
     *        items are required.
     * @param {object} options
     *        Additional options for the request containing following properties:
     *         - offset {number} : The begin index of the returned array amongst
     *                  the total values
     *         - size {number} : The number of values required.
     *         - sortOn {string} : The values should be sorted on this property.
     *
     * @return {object} An object containing following properties:
     *          - offset - The actual offset of the returned array. This might
     *                     be different from the requested offset if that was
     *                     invalid
     *          - size - The actual size of the returned array.
     *          - data - The requested values.
     */
    getStoreObjects: method(function(host, names, options = {}) {
      let offset = options.offset || 0;
      let size = options.offset || MAX_STORE_OBJECT_COUNT;
      let sortOn = options.sortOn || "name";

      let toReturn = {
        offset: offset,
        total: -1,
        data: []
      };

      if (names) {
        for (let name of names) {
          toReturn.data.push(
            this.toStoreObject(this.getValuesForHost(host, name))
          );
        }
      }
      else {
        let total = this.getValuesForHost(host);
        toReturn.total = total.length;
        if (offset > toReturn.total) {
          toReturn.offset = offset = 0;
        }

        toReturn.data = total.sort((a,b) => {
          return a[sortOn] - b[sortOn];
        }).slice(offset, size).map(object => this.toStoreObject(object));
      }

      return toReturn;
    }, {
      request: {
        host: Arg(0),
        names: Arg(1, "nullable:array:string"),
        options: Arg(2, "nullable:json")
      },
      response: RetVal(storeObjectType)
    })
  }
};

/**
 * Creates an actor and its corresponding front and registers it to the Storage
 * Actor.
 *
 * @See StorageActors.defaults()
 *
 * @param {object} options
 *        Options required by StorageActors.defaults method which are :
 *         - typeName {string}
 *                    The typeName of the actor.
 *         - observationTopic {string}
 *                            The topic which this actor listens to via
 *                            Notification Observers.
 *         - storeObjectType {string}
 *                           The RetVal type of the store object of this actor.
 * @param {object} overrides
 *        All the methods which you want to be differnt from the ones in
 *        StorageActors.defaults method plus the required ones described there.
 */
StorageActors.createActor = function(options = {}, overrides = {}) {
  let actorObject = StorageActors.defaults(
    options.typeName,
    options.observationTopic,
    options.storeObjectType
  );
  for (let key in overrides) {
    actorObject[key] = overrides[key];
  }

  let actor = protocol.ActorClass(actorObject);
  let front = protocol.FrontClass(actor, {
    form: function(form, detail) {
      if (detail === "actorid") {
        this.actorID = form;
        return null;
      }

      this.actorID = form.actor;
      this.hosts = form.hosts;
      return null;
    }
  });
  storageTypePool.set(actorObject.typeName, actor);
}

/**
 * The Cookies actor and front.
 */
StorageActors.createActor({
  typeName: "cookies",
  observationTopic: "cookie-changed",
  storeObjectType: "cookiestoreobject"
}, {
  initialize: function(storageActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.storageActor = storageActor;

    this.populateStoresForHosts();
    Services.obs.addObserver(this, "cookie-changed", false);
    Services.obs.addObserver(this, "http-on-response-set-cookie", false);
    this.onWindowReady = this.onWindowReady.bind(this);
    this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
    events.on(this.storageActor, "window-ready", this.onWindowReady);
    events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
  },

  destroy: function() {
    this.hostVsStores = null;
    Services.obs.removeObserver(this, "cookie-changed", false);
    Services.obs.removeObserver(this, "http-on-response-set-cookie", false);
    events.off(this.storageActor, "window-ready", this.onWindowReady);
    events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);
  },

  getNamesForHost: function(host) {
    return [...this.hostVsStores.get(host).keys()];
  },

  getValuesForHost: function(host, name) {
    if (name) {
      return this.hostVsStores.get(host).get(name);
    }
    return [...this.hostVsStores.get(host).values()];
  },

  /**
   * Given a cookie object, figure out all the matching hosts from the page that
   * the cookie belong to.
   */
  getMatchingHosts: function(cookies) {
    if (!cookies.length) {
      cookies = [cookies];
    }
    let hosts = new Set();
    for (let host of this.hosts) {
      for (let cookie of cookies) {
        if (this.isCookieAtHost(cookie, host)) {
          hosts.add(host);
        }
      }
    }
    return [...hosts];
  },

  /**
   * Given a cookie object and a host, figure out if the cookie is valid for
   * that host.
   */
  isCookieAtHost: function(cookie, host) {
    try {
      cookie = cookie.QueryInterface(Ci.nsICookie)
                     .QueryInterface(Ci.nsICookie2);
    } catch(ex) {
      return false;
    }
    if (cookie.host == null) {
      return host == null;
    }
    if (cookie.host.startsWith(".")) {
      return host.endsWith(cookie.host);
    }
    else {
      return cookie.host == host;
    }
  },

  toStoreObject: function(cookie) {
    if (!cookie) {
      return null;
    }

    return {
      name: cookie.name,
      path: cookie.path || "",
      host: cookie.host || "",
      expires: (cookie.expires || 0) * 1000, // because expires is in seconds
      creationTime: cookie.creationTime / 1000, // because it is in micro seconds
      lastAccessed: cookie.lastAccessed / 1000, // - do -
      value: new LongStringActor(this.conn, cookie.value || ""),
      isDomain: cookie.isDomain,
      isSecure: cookie.isSecure,
      isHttpOnly: cookie.isHttpOnly
    }
  },

  populateStoresForHost: function(host) {
    this.hostVsStores.set(host, new Map());
    let cookies = Services.cookies.getCookiesFromHost(host);
    while (cookies.hasMoreElements()) {
      let cookie = cookies.getNext().QueryInterface(Ci.nsICookie)
                          .QueryInterface(Ci.nsICookie2);
      if (this.isCookieAtHost(cookie, host)) {
        this.hostVsStores.get(host).set(cookie.name, cookie);
      }
    }
  },

  /**
   * Converts the raw cookie string returned in http request's response header
   * to a nsICookie compatible object.
   *
   * @param {string} cookieString
   *        The raw cookie string coming from response header.
   * @param {string} domain
   *        The domain of the url of the nsiChannel the cookie corresponds to.
   *        This will be used when the cookie string does not have a domain.
   *
   * @returns {[object]}
   *          An array of nsICookie like objects representing the cookies.
   */
  parseCookieString: function(cookieString, domain) {
    /**
     * Takes a date string present in raw cookie string coming from http
     * request's response headers and returns the number of milliseconds elapsed
     * since epoch. If the date string is undefined, its probably a session
     * cookie so return 0.
     */
    let parseDateString = dateString => {
      return dateString ? new Date(dateString.replace(/-/g, " ")).getTime(): 0;
    };

    let cookies = [];
    for (let string of cookieString.split("\n")) {
      let keyVals = {}, name = null;
      for (let keyVal of string.split(/;\s*/)) {
        let tokens = keyVal.split(/\s*=\s*/);
        if (!name) {
          name = tokens[0];
        }
        else {
          tokens[0] = tokens[0].toLowerCase();
        }
        keyVals[tokens.splice(0, 1)[0]] = tokens.join("=");
      }
      let expiresTime = parseDateString(keyVals.expires);
      keyVals.domain = keyVals.domain || domain;
      cookies.push({
        name: name,
        value: keyVals[name] || "",
        path: keyVals.path,
        host: keyVals.domain,
        expires: expiresTime/1000, // seconds, to match with nsiCookie.expires
        lastAccessed: expiresTime * 1000,
        // microseconds, to match with nsiCookie.lastAccessed
        creationTime: expiresTime * 1000,
        // microseconds, to match with nsiCookie.creationTime
        isHttpOnly: true,
        isSecure: keyVals.secure != null,
        isDomain: keyVals.domain.startsWith("."),
      });
    }
    return cookies;
  },

  /**
   * Notification observer for topics "http-on-response-set-cookie" and
   * "cookie-change".
   *
   * @param subject
   *        {nsiChannel} The channel associated to the SET-COOKIE response
   *        header in case of "http-on-response-set-cookie" topic.
   *        {nsiCookie|[nsiCookie]} A single nsiCookie object or a list of it
   *        depending on the action. Array is only in case of "batch-deleted"
   *        action.
   * @param {string} topic
   *        The topic of the notification.
   * @param {string} action
   *        Additional data associated with the notification. Its the type of
   *        cookie change in case of "cookie-change" topic and the cookie string
   *        in case of "http-on-response-set-cookie" topic.
   */
  observe: function(subject, topic, action) {
    if (topic == "http-on-response-set-cookie") {
      // Some cookies got created as a result of http response header SET-COOKIE
      // subject here is an nsIChannel object referring to the http request.
      // We get the requestor of this channel and thus the content window.
      let channel = subject.QueryInterface(Ci.nsIChannel);
      let requestor = channel.notificationCallbacks ||
                      channel.loadGroup.notificationCallbacks;
      // requester can be null sometimes.
      let window = requestor ? requestor.getInterface(Ci.nsIDOMWindow): null;
      // Proceed only if this window is present on the currently targetted tab
      if (window && this.storageActor.isIncludedInTopLevelWindow(window)) {
        let host = this.getHostName(window.location);
        if (this.hostVsStores.has(host)) {
          let cookies = this.parseCookieString(action, channel.URI.host);
          let data = {};
          data[host] =  [];
          for (let cookie of cookies) {
            if (this.hostVsStores.get(host).has(cookie.name)) {
              continue;
            }
            this.hostVsStores.get(host).set(cookie.name, cookie);
            data[host].push(cookie.name);
          }
          if (data[host]) {
            this.storageActor.update("added", "cookies", data);
          }
        }
      }
      return null;
    }

    if (topic != "cookie-changed") {
      return null;
    }

    let hosts = this.getMatchingHosts(subject);
    let data = {};

    switch(action) {
      case "added":
      case "changed":
        if (hosts.length) {
          subject = subject.QueryInterface(Ci.nsICookie)
                           .QueryInterface(Ci.nsICookie2);
          for (let host of hosts) {
            this.hostVsStores.get(host).set(subject.name, subject);
            data[host] = [subject.name];
          }
          this.storageActor.update(action, "cookies", data);
        }
        break;

      case "deleted":
        if (hosts.length) {
          subject = subject.QueryInterface(Ci.nsICookie)
                           .QueryInterface(Ci.nsICookie2);
          for (let host of hosts) {
            this.hostVsStores.get(host).delete(subject.name);
            data[host] = [subject.name];
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case "batch-deleted":
        if (hosts.length) {
          for (let host of hosts) {
            let stores = [];
            for (let cookie of subject) {
              cookie = cookie.QueryInterface(Ci.nsICookie)
                             .QueryInterface(Ci.nsICookie2);
              this.hostVsStores.get(host).delete(cookie.name);
              stores.push(cookie.name);
            }
            data[host] = stores;
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case "cleared":
        this.storageActor.update("cleared", "cookies", hosts);
        break;

      case "reload":
        this.storageActor.update("reloaded", "cookies", hosts);
        break;
    }
    return null;
  },
});


/**
 * Helper method to create the overriden object required in
 * StorageActors.createActor for Local Storage and Session Storage.
 * This method exists as both Local Storage and Session Storage have almost
 * identical actors.
 */
function getObjectForLocalOrSessionStorage(type) {
  return {
    getNamesForHost: function(host) {
      let storage = this.hostVsStores.get(host);
      return [key for (key in storage)];
    },

    getValuesForHost: function(host, name) {
      let storage = this.hostVsStores.get(host);
      if (name) {
        return {name: name, value: storage.getItem(name)};
      }
      return [{name: name, value: storage.getItem(name)} for (name in storage)];
    },

    getHostName: function(location) {
      if (!location.host) {
        return location.href;
      }
      return location.protocol + "//" + location.host;
    },

    populateStoresForHost: function(host, window) {
      try {
        this.hostVsStores.set(host, window[type]);
      } catch(ex) {
        // Exceptions happen when local or session storage is inaccessible
      }
      return null;
    },

    populateStoresForHosts: function() {
      this.hostVsStores = new Map();
      try {
        for (let window of this.windows) {
          this.hostVsStores.set(this.getHostName(window.location), window[type]);
        }
      } catch(ex) {
        // Exceptions happen when local or session storage is inaccessible
      }
      return null;
    },

    observe: function(subject, topic, data) {
      if (topic != "dom-storage2-changed" || data != type) {
        return null;
      }

      let host = this.getSchemaAndHost(subject.url);

      if (!this.hostVsStores.has(host)) {
        return null;
      }

      let action = "changed";
      if (subject.key == null) {
        return this.storageActor.update("cleared", type, [host]);
      }
      else if (subject.oldValue == null) {
        action = "added";
      }
      else if (subject.newValue == null) {
        action = "deleted";
      }
      let updateData = {};
      updateData[host] = [subject.key];
      return this.storageActor.update(action, type, updateData);
    },

    /**
     * Given a url, correctly determine its protocol + hostname part.
     */
    getSchemaAndHost: function(url) {
      let uri = Services.io.newURI(url, null, null);
      return uri.scheme + "://" + uri.hostPort;
    },

    toStoreObject: function(item) {
      if (!item) {
        return null;
      }

      return {
        name: item.name,
        value: new LongStringActor(this.conn, item.value || "")
      };
    },
  }
};

/**
 * The Local Storage actor and front.
 */
StorageActors.createActor({
  typeName: "localStorage",
  observationTopic: "dom-storage2-changed",
  storeObjectType: "storagestoreobject"
}, getObjectForLocalOrSessionStorage("localStorage"));

/**
 * The Session Storage actor and front.
 */
StorageActors.createActor({
  typeName: "sessionStorage",
  observationTopic: "dom-storage2-changed",
  storeObjectType: "storagestoreobject"
}, getObjectForLocalOrSessionStorage("sessionStorage"));


/**
 * The main Storage Actor.
 */
let StorageActor = exports.StorageActor = protocol.ActorClass({
  typeName: "storage",

  get window() {
    return this.parentActor.window;
  },

  get document() {
    return this.parentActor.window.document;
  },

  get windows() {
    return this.childWindowPool;
  },

  /**
   * List of event notifications that the server can send to the client.
   *
   *  - stores-update : When any store object in any storage type changes.
   *  - stores-cleared : When all the store objects are removed.
   *  - stores-reloaded : When all stores are reloaded. This generally mean that
   *                      we should refetch everything again.
   */
  events: {
    "stores-update": {
      type: "storesUpdate",
      data: Arg(0, "storeUpdateObject")
    },
    "stores-cleared": {
      type: "storesCleared",
      data: Arg(0, "json")
    },
    "stores-reloaded": {
      type: "storesRelaoded",
      data: Arg(0, "json")
    }
  },

  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.conn = conn;
    this.parentActor = tabActor;

    this.childActorPool = new Map();
    this.childWindowPool = new Set();

    // Get the top level content docshell for the tab we are targetting.
    this.topDocshell = tabActor.window
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShell)
      .QueryInterface(Ci.nsIDocShellTreeItem);
    // Fetch all the inner iframe windows in this tab.
    this.fetchChildWindows(this.topDocshell);

    // Initialize the registered store types
    for (let [store, actor] of storageTypePool) {
      this.childActorPool.set(store, new actor(this));
    }

    // Notifications that help us keep track of newly added windows and windows
    // that got removed
    Services.obs.addObserver(this, "content-document-global-created", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    this.onPageChange = this.onPageChange.bind(this);
    tabActor.browser.addEventListener("pageshow", this.onPageChange, true);
    tabActor.browser.addEventListener("pagehide", this.onPageChange, true);

    this.boundUpdate = {};
    // The time which periodically flushes and transfers the updated store
    // objects.
    this.updateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.updateTimer.initWithCallback(this , UPDATE_INTERVAL,
      Ci.nsITimer.TYPE_REPEATING_PRECISE_CAN_SKIP);

    // Layout helper for window.parent and window.top helper methods that work
    // accross devices.
    this.layoutHelper = new LayoutHelpers(this.window);
  },

  destroy: function() {
    this.updateTimer.cancel();
    this.updateTimer = null;
    // Remove observers
    Services.obs.removeObserver(this, "content-document-global-created", false);
    Services.obs.removeObserver(this, "inner-window-destroyed", false);
    if (this.parentActor.browser) {
      this.parentActor.browser.removeEventListener(
        "pageshow", this.onPageChange, true);
      this.parentActor.browser.removeEventListener(
        "pagehide", this.onPageChange, true);
    }
    // Destroy the registered store types
    for (let actor of this.childActorPool.values()) {
      actor.destroy();
    }
    this.childActorPool.clear();
    this.topDocshell = null;
  },

  /**
   * Given a docshell, recursively find otu all the child windows from it.
   *
   * @param {nsIDocShell} item
   *        The docshell from which all inner windows need to be extracted.
   */
  fetchChildWindows: function(item) {
    let docShell = item.QueryInterface(Ci.nsIDocShell)
                       .QueryInterface(Ci.nsIDocShellTreeItem);
    if (!docShell.contentViewer) {
      return null;
    }
    let window = docShell.contentViewer.DOMDocument.defaultView;
    if (window.location.href == "about:blank") {
      // Skip out about:blank windows as Gecko creates them multiple times while
      // creating any global.
      return null;
    }
    this.childWindowPool.add(window);
    for (let i = 0; i < docShell.childCount; i++) {
      let child = docShell.getChildAt(i);
      this.fetchChildWindows(child);
    }
    return null;
  },

  isIncludedInTopLevelWindow: function(window) {
    return this.layoutHelper.isIncludedInTopLevelWindow(window);
  },

  getWindowFromInnerWindowID: function(innerID) {
    innerID = innerID.QueryInterface(Ci.nsISupportsPRUint64).data;
    for (let win of this.childWindowPool.values()) {
      let id = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .currentInnerWindowID;
      if (id == innerID) {
        return win;
      }
    }
    return null;
  },

  /**
   * Event handler for any docshell update. This lets us figure out whenever
   * any new window is added, or an existing window is removed.
   */
  observe: function(subject, topic, data) {
    if (subject.location &&
        (!subject.location.href || subject.location.href == "about:blank")) {
      return null;
    }
    if (topic == "content-document-global-created" &&
        this.isIncludedInTopLevelWindow(subject)) {
      this.childWindowPool.add(subject);
      events.emit(this, "window-ready", subject);
    }
    else if (topic == "inner-window-destroyed") {
      let window = this.getWindowFromInnerWindowID(subject);
      if (window) {
        this.childActorPool.delete(window);
        events.emit(this, "window-destroyed", window);
      }
    }
    return null;
  },

  onPageChange: function({target, type}) {
    let window = target.defaultView;
    if (type == "pagehide" && this.childWindowPool.delete(window)) {
      events.emit(this, "window-destroyed", window)
    }
    else if (type == "pageshow" && window.location.href &&
             window.location.href != "about:blank" &&
             this.isIncludedInTopLevelWindow(window)) {
      this.childWindowPool.add(window);
      events.emit(this, "window-ready", window);
    }
  },

  /**
   * Lists the availabel hosts for all the registered storage types.
   *
   * @returns {object} An object containing with the following structure:
   *  - <storageType> : [{
   *      actor: <actorId>,
   *      host: <hostname>
   *    }]
   */
  listStores: method(function() {
    // Explictly recalculate the window-tree
    this.childWindowPool.clear();
    this.fetchChildWindows(this.topDocshell);

    let toReturn = {};
    for (let [name, value] of this.childActorPool) {
      toReturn[name] = value;
    }
    return toReturn;
  }, {
    response: RetVal(types.addDictType("storelist", getRegisteredTypes()))
  }),

  /**
   * Notifies the client front with the updates in stores at regular intervals.
   */
  notify: function() {
    if (!this.updatePending || this.updatingUpdateObject) {
      return null;
    }
    events.emit(this, "stores-update", this.boundUpdate);
    this.boundUpdate = {};
    this.updatePending = false;
    return null;
  },

  /**
   * This method is called by the registered storage types so as to tell the
   * Storage Actor that there are some changes in the stores. Storage Actor then
   * notifies the client front about these changes at regular (UPDATE_INTERVAL)
   * interval.
   *
   * @param {string} action
   *        The type of change. One of "added", "changed" or "deleted"
   * @param {string} storeType
   *        The storage actor in which this change has occurred.
   * @param {object} data
   *        The update object. This object is of the following format:
   *         - {
   *             <host1>: [<store_names1>, <store_name2>...],
   *             <host2>: [<store_names34>...],
   *           }
   *           Where host1, host2 are the host in which this change happened and
   *           [<store_namesX] is an array of the names of the changed store
   *           objects. Leave it empty if the host was completely removed.
   *        When the action is "reloaded" or "cleared", `data` is an array of
   *        hosts for which the stores were cleared or reloaded.
   */
  update: function(action, storeType, data) {
    if (action == "cleared" || action == "reloaded") {
      let toSend = {};
      toSend[storeType] = data
      events.emit(this, "stores-" + action, toSend);
      return null;
    }

    this.updatingUpdateObject = true;
    if (!this.boundUpdate[action]) {
      this.boundUpdate[action] = {};
    }
    if (!this.boundUpdate[action][storeType]) {
      this.boundUpdate[action][storeType] = {};
    }
    this.updatePending = true;
    for (let host in data) {
      if (!this.boundUpdate[action][storeType][host] || action == "deleted") {
        this.boundUpdate[action][storeType][host] = data[host];
      }
      else {
        this.boundUpdate[action][storeType][host] =
        this.boundUpdate[action][storeType][host].concat(data[host]);
      }
    }
    if (action == "added") {
      // If the same store name was previously deleted or changed, but now is
      // added somehow, dont send the deleted or changed update.
      this.removeNamesFromUpdateList("deleted", storeType, data);
      this.removeNamesFromUpdateList("changed", storeType, data);
    }
    else if (action == "changed" && this.boundUpdate.added &&
             this.boundUpdate.added[storeType]) {
      // If something got added and changed at the same time, then remove those
      // items from changed instead.
      this.removeNamesFromUpdateList("changed", storeType,
                                     this.boundUpdate.added[storeType]);
    }
    else if (action == "deleted") {
      // If any item got delete, or a host got delete, no point in sending
      // added or changed update
      this.removeNamesFromUpdateList("added", storeType, data);
      this.removeNamesFromUpdateList("changed", storeType, data);
      for (let host in data) {
        if (data[host].length == 0 && this.boundUpdate.added &&
            this.boundUpdate.added[storeType] &&
            this.boundUpdate.added[storeType][host]) {
          delete this.boundUpdate.added[storeType][host];
        }
        if (data[host].length == 0 && this.boundUpdate.changed &&
            this.boundUpdate.changed[storeType] &&
            this.boundUpdate.changed[storeType][host]) {
          delete this.boundUpdate.changed[storeType][host];
        }
      }
    }
    this.updatingUpdateObject = false;
    return null;
  },

  /**
   * This method removes data from the this.boundUpdate object in the same
   * manner like this.update() adds data to it.
   *
   * @param {string} action
   *        The type of change. One of "added", "changed" or "deleted"
   * @param {string} storeType
   *        The storage actor for which you want to remove the updates data.
   * @param {object} data
   *        The update object. This object is of the following format:
   *         - {
   *             <host1>: [<store_names1>, <store_name2>...],
   *             <host2>: [<store_names34>...],
   *           }
   *           Where host1, host2 are the hosts which you want to remove and
   *           [<store_namesX] is an array of the names of the store objects.
   */
  removeNamesFromUpdateList: function(action, storeType, data) {
    for (let host in data) {
      if (this.boundUpdate[action] && this.boundUpdate[action][storeType] &&
          this.boundUpdate[action][storeType][host]) {
        for (let name in data[host]) {
          let index = this.boundUpdate[action][storeType][host].indexOf(name);
          if (index > -1) {
            this.boundUpdate[action][storeType][host].splice(index, 1);
          }
        }
        if (!this.boundUpdate[action][storeType][host].length) {
          delete this.boundUpdate[action][storeType][host];
        }
      }
    }
    return null;
  }
});

/**
 * Front for the Storage Actor.
 */
let StorageFront = exports.StorageFront = protocol.FrontClass(StorageActor, {
  initialize: function(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.storageActor;

    client.addActorPool(this);
    this.manage(this);
  }
});
