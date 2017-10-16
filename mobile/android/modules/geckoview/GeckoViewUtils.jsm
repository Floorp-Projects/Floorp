/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
});

this.EXPORTED_SYMBOLS = ["GeckoViewUtils"];

var GeckoViewUtils = {
  /**
   * Define a lazy getter that loads an object from external code, and
   * optionally handles observer and/or message manager notifications for the
   * object, so the object only loads when a notification is received.
   *
   * @param scope     Scope for holding the loaded object.
   * @param name      Name of the object to load.
   * @param service   If specified, load the object from a JS component; the
   *                  component must include the line
   *                  "this.wrappedJSObject = this;" in its constructor.
   * @param module    If specified, load the object from a JS module.
   * @param init      Optional post-load initialization function.
   * @param observers If specified, listen to specified observer notifications.
   * @param ppmm      If specified, listen to specified process messages.
   * @param mm        If specified, listen to specified frame messages.
   * @param ged       If specified, listen to specified global EventDispatcher events.
   * @param once      if true, only listen to the specified
   *                  events/messages/notifications once.
   */
  addLazyGetter: function(scope, name, {service, module, handler,
                                        observers, ppmm, mm, ged, init, once}) {
    XPCOMUtils.defineLazyGetter(scope, name, _ => {
      let ret = undefined;
      if (module) {
        ret = Cu.import(module, {})[name];
      } else if (service) {
        ret = Cc[service].getService(Ci.nsISupports).wrappedJSObject;
      } else if (typeof handler === "function") {
        ret = {
          handleEvent: handler,
          observe: handler,
          onEvent: handler,
          receiveMessage: handler,
        };
      } else if (handler) {
        ret = handler;
      }
      if (ret && init) {
        init.call(scope, ret);
      }
      return ret;
    });

    if (observers) {
      let observer = (subject, topic, data) => {
        Services.obs.removeObserver(observer, topic);
        if (!once) {
          Services.obs.addObserver(scope[name], topic);
        }
        scope[name].observe(subject, topic, data); // Explicitly notify new observer
      };
      observers.forEach(topic => Services.obs.addObserver(observer, topic));
    }

    let addMMListener = (target, names) => {
      let listener = msg => {
        target.removeMessageListener(msg.name, listener);
        if (!once) {
          target.addMessageListener(msg.name, scope[name]);
        }
        scope[name].receiveMessage(msg);
      };
      names.forEach(msg => target.addMessageListener(msg, listener));
    };
    if (ppmm) {
      addMMListener(Services.ppmm, ppmm);
    }
    if (mm) {
      addMMListener(Services.mm, mm);
    }

    if (ged) {
      let listener = (event, data, callback) => {
        EventDispatcher.instance.unregisterListener(listener, event);
        if (!once) {
          EventDispatcher.instance.registerListener(scope[name], event);
        }
        scope[name].onEvent(event, data, callback);
      };
      EventDispatcher.instance.registerListener(listener, ged);
    }
  },

  _addLazyListeners: function(events, handler, scope, name, addFn, handleFn) {
    if (!handler) {
      handler = (_ => Array.isArray(name) ? name.map(n => scope[n]) : scope[name]);
    }
    let listener = (...args) => {
      let handlers = handler(...args);
      if (!handlers) {
          return;
      }
      if (!Array.isArray(handlers)) {
        handlers = [handlers];
      }
      handleFn(handlers, listener, args);
    };
    if (Array.isArray(events)) {
      addFn(events, listener);
    } else {
      addFn([events], listener);
    }
  },

  /**
   * Add lazy event listeners that only load the actual handler when an event
   * is being handled.
   *
   * @param target  Event target for the event listeners.
   * @param events  Event name as a string or array.
   * @param handler If specified, function that, for a given event, returns the
   *                actual event handler as an object or an array of objects.
   *                If handler is not specified, the actual event handler is
   *                specified using the scope and name pair.
   * @param scope   See handler.
   * @param name    See handler.
   * @param options Options for addEventListener.
   */
  addLazyEventListener: function(target, events, {handler, scope, name, options}) {
    this._addLazyListeners(events, handler, scope, name,
      (events, listener) => {
        events.forEach(event => target.addEventListener(event, listener, options));
      },
      (handlers, listener, args) => {
        if (!options || !options.once) {
          target.removeEventListener(args[0].type, listener, options);
          handlers.forEach(handler =>
            target.addEventListener(args[0].type, handler, options));
        }
        handlers.forEach(handler => handler.handleEvent(args[0]));
      });
  },

  /**
   * Add lazy event listeners on the per-window EventDispatcher, and only load
   * the actual handler when an event is being handled.
   *
   * @param window  Window with the target EventDispatcher.
   * @param events  Event name as a string or array.
   * @param handler If specified, function that, for a given event, returns the
   *                actual event handler as an object or an array of objects.
   *                If handler is not specified, the actual event handler is
   *                specified using the scope and name pair.
   * @param scope   See handler.
   * @param name    See handler.
   * @param once    If true, only listen to the specified events once.
   */
  registerLazyWindowEventListener: function(window, events,
                                            {handler, scope, name, once}) {
    let dispatcher = this.getDispatcherForWindow(window);

    this._addLazyListeners(events, handler, scope, name,
      (events, listener) => {
        dispatcher.registerListener(listener, events);
      },
      (handlers, listener, args) => {
        if (!once) {
          dispatcher.unregisterListener(listener, args[0]);
          handlers.forEach(handler =>
            dispatcher.registerListener(handler, args[0]));
        }
        handlers.forEach(handler => handler.onEvent(...args));
      });
  },

  /**
   * Return the outermost chrome DOM window (the XUL window) for a given DOM
   * window.
   *
   * @param aWin a DOM window.
   */
  getChromeWindow: function(aWin) {
    if (!aWin) {
      return null;
    }
    let docShell;
    try {
      docShell = aWin.QueryInterface(Ci.nsIDocShell);
    } catch (e) {
      docShell = aWin.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDocShell);
    }
    return docShell.QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
  },

  /**
   * Return the per-nsWindow EventDispatcher for a given DOM window.
   *
   * @param aWin a DOM window.
   */
  getDispatcherForWindow: function(aWin) {
    try {
      let win = this.getChromeWindow(aWin.top || aWin);
      let dispatcher = win.WindowEventDispatcher || EventDispatcher.for(win);
      if (!win.closed && dispatcher) {
        return dispatcher;
      }
    } catch (e) {
      return null;
    }
  },

  getActiveDispatcher: function() {
    let dispatcher = this.getDispatcherForWindow(Services.focus.activeWindow);
    if (dispatcher) {
      return dispatcher;
    }

    let iter = Services.wm.getEnumerator(/* windowType */ null);
    while (iter.hasMoreElements()) {
      dispatcher = this.getDispatcherForWindow(
          iter.getNext().QueryInterface(Ci.nsIDOMWindow));
      if (dispatcher) {
        return dispatcher;
      }
    }
    return null;
  },
};
