/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains event parsers that are then used by developer tools in
// order to find information about events affecting an HTML element.

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyGetter(this, "eventListenerService", () => {
  return Cc["@mozilla.org/eventlistenerservice;1"]
           .getService(Ci.nsIEventListenerService);
});

let parsers = [
  {
    id: "jQuery events",
    getListeners: function(node) {
      let global = node.ownerGlobal.wrappedJSObject;
      let hasJQuery = global.jQuery && global.jQuery.fn && global.jQuery.fn.jquery;

      if (!hasJQuery) {
        return;
      }

      let jQuery = global.jQuery;
      let handlers = [];

      // jQuery 1.2+
      let data = jQuery._data || jQuery.data;
      if (data) {
        let eventsObj = data(node, "events");
        for (let type in eventsObj) {
          let events = eventsObj[type];
          for (let key in events) {
            let event = events[key];
            if (typeof event === "object" || typeof event === "function") {
              let eventInfo = {
                type: type,
                handler: event.handler || event,
                tags: "jQuery",
                hide: {
                  capturing: true,
                  dom0: true
                }
              };

              handlers.push(eventInfo);
            }
          }
        }
      }

      // JQuery 1.0 & 1.1
      let entry = jQuery(node)[0];

      if (!entry) {
        return handlers;
      }

      for (let type in entry.events) {
        let events = entry.events[type];
        for (let key in events) {
          if (typeof events[key] === "function") {
            let eventInfo = {
              type: type,
              handler: events[key],
              tags: "jQuery",
              hide: {
                capturing: true,
                dom0: true
              }
            };

            handlers.push(eventInfo);
          }
        }
      }

      return handlers;
    }
  },
  {
    id: "jQuery live events",
    hasListeners: function(node) {
      return jQueryLiveGetListeners(node, true);
    },
    getListeners: function(node) {
      return jQueryLiveGetListeners(node, false);
    },
    normalizeHandler: function(handlerDO) {
      let paths = [
        [".event.proxy/", ".event.proxy/", "*"],
        [".proxy/", "*"]
      ];

      let name = handlerDO.displayName;

      if (!name) {
        return handlerDO;
      }

      for (let path of paths) {
        if (name.contains(path[0])) {
          path.splice(0, 1);

          for (let point of path) {
            let names = handlerDO.environment.names();

            for (let varName of names) {
              let temp = handlerDO.environment.getVariable(varName);
              if (!temp) {
                continue;
              }

              let displayName = temp.displayName;
              if (!displayName) {
                continue;
              }

              if (temp.class === "Function" &&
                  (displayName.contains(point) || point === "*")) {
                handlerDO = temp;
                break;
              }
            }
          }
          break;
        }
      }

      return handlerDO;
    }
  },
  {
    id: "DOM events",
    hasListeners: function(node) {
      let listeners;

      if (node.nodeName.toLowerCase() === "html") {
        listeners = eventListenerService.getListenerInfoFor(node.ownerGlobal) || [];
      } else {
        listeners = eventListenerService.getListenerInfoFor(node) || [];
      }

      for (let listener of listeners) {
        if (listener.listenerObject && listener.type) {
          return true;
        }
      }

      return false;
    },
    getListeners: function(node) {
      let handlers = [];
      let listeners = eventListenerService.getListenerInfoFor(node);

      // The Node actor's getEventListenerInfo knows that when an html tag has
      // been passed we need the window object so we don't need to account for
      // event hoisting here as we did in hasListeners.

      for (let listenerObj of listeners) {
        let listener = listenerObj.listenerObject;

        // If there is no JS event listener skip this.
        if (!listener) {
          continue;
        }

        let eventInfo = {
          capturing: listenerObj.capturing,
          type: listenerObj.type,
          handler: listener
        };

        handlers.push(eventInfo);
      }

      return handlers;
    }
  }
];

function jQueryLiveGetListeners(node, boolOnEventFound) {
  let global = node.ownerGlobal.wrappedJSObject;
  let hasJQuery = global.jQuery && global.jQuery.fn && global.jQuery.fn.jquery;

  if (!hasJQuery) {
    return;
  }

  let jQuery = global.jQuery;
  let handlers = [];
  let data = jQuery._data || jQuery.data;

  if (data) {
    // Live events are added to the document and bubble up to all elements.
    // Any element matching the specified selector will trigger the live
    // event.
    let events = data(global.document, "events");

    for (let type in events) {
      let eventHolder = events[type];

      for (let idx in eventHolder) {
        if (typeof idx !== "string" || isNaN(parseInt(idx, 10))) {
          continue;
        }

        let event = eventHolder[idx];
        let selector = event.selector;

        if (!selector && event.data) {
          selector = event.data.selector || event.data || event.selector;
        }

        if (!selector || !node.ownerDocument) {
          continue;
        }

        let matches = node.matches && node.matches(selector);
        if (boolOnEventFound && matches) {
          return true;
        }

        if (!matches) {
          continue;
        }

        if (!boolOnEventFound && (typeof event === "object" || typeof event === "function")) {
          let eventInfo = {
            type: event.origType || event.type.substr(selector.length + 1),
            handler: event.handler || event,
            tags: "jQuery,Live",
            hide: {
              dom0: true,
              capturing: true
            }
          };

          if (!eventInfo.type && event.data && event.data.live) {
            eventInfo.type = event.data.live;
          }

          handlers.push(eventInfo);
        }
      }
    }
  }

  if (boolOnEventFound) {
    return false;
  }
  return handlers;
}

this.EventParsers = function EventParsers() {
  if (this._eventParsers.size === 0) {
    for (let parserObj of parsers) {
      this.registerEventParser(parserObj);
    }
  }
};

exports.EventParsers = EventParsers;

EventParsers.prototype = {
  _eventParsers: new Map(), // NOTE: This is shared amongst all instances.

  get parsers() {
    return this._eventParsers;
  },

  /**
   * Register a new event parser to be used in the processing of event info.
   *
   * @param {Object} parserObj
   *        Each parser must contain the following properties:
   *        - parser, which must take the following form:
   *   {
   *     id {String}: "jQuery events",         // Unique id.
   *     getListeners: function(node) { },     // Function that takes a node and
   *                                           // returns an array of eventInfo
   *                                           // objects (see below).
   *
   *     hasListeners: function(node) { },     // Optional function that takes a
   *                                           // node and returns a boolean
   *                                           // indicating whether a node has
   *                                           // listeners attached.
   *
   *     normalizeHandler: function(fnDO) { }, // Optional function that takes a
   *                                           // Debugger.Object instance and
   *                                           // climbs the scope chain to get
   *                                           // the function that should be
   *                                           // displayed in the event bubble
   *                                           // see the following url for
   *                                           // details:
   *                                           //   https://developer.mozilla.org/
   *                                           //   docs/Tools/Debugger-API/
   *                                           //   Debugger.Object
   *   }
   *
   * An eventInfo object should take the following form:
   *   {
   *     type {String}:      "click",
   *     handler {Function}: event handler,
   *     tags {String}:      "jQuery,Live", // These tags will be displayed as
   *                                        // attributes in the events popup.
   *     hide: {               // Hide or show fields:
   *       debugger: false,    // Debugger icon
   *       type: false,        // Event type e.g. click
   *       filename: false,    // Filename
   *       capturing: false,   // Capturing
   *       dom0: false         // DOM 0
   *     },
   *
   *     override: {                        // The following can be overridden:
   *       type: "click",
   *       origin: "http://www.mozilla.com",
   *       searchString: 'onclick="doSomething()"',
   *       DOM0: true,
   *       capturing: true
   *     }
   *   }
   */
  registerEventParser: function(parserObj) {
    let parserId = parserObj.id;

    if (!parserId) {
      throw new Error("Cannot register new event parser with id " + parserId);
    }
    if (this._eventParsers.has(parserId)) {
      throw new Error("Duplicate event parser id " + parserId);
    }

    this._eventParsers.set(parserId, {
      getListeners: parserObj.getListeners,
      hasListeners: parserObj.hasListeners,
      normalizeHandler: parserObj.normalizeHandler
    });
  },

  /**
   * Removes parser that matches a given parserId.
   *
   * @param {String} parserId
   *        id of the event parser to unregister.
   */
  unregisterEventParser: function(parserId) {
    this._eventParsers.delete(parserId);
  },

  /**
   * Tidy up parsers.
   */
  destroy: function() {
    for (let [id] of this._eventParsers) {
      this.unregisterEventParser(id, true);
    }
  }
};
