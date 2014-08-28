/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains event parsers that are then used by developer tools in
// order to find information about events affecting an HTML element.

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

loader.lazyGetter(this, "eventListenerService", () => {
  return Cc["@mozilla.org/eventlistenerservice;1"]
           .getService(Ci.nsIEventListenerService);
});

let eventParsers = [
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

for (let parserObj of eventParsers) {
  gDevTools.registerEventParser(parserObj);
}

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
