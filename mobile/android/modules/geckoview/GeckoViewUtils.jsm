/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AndroidLog: "resource://gre/modules/AndroidLog.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Log: "resource://gre/modules/Log.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

var EXPORTED_SYMBOLS = ["GeckoViewUtils"];

/**
 * A formatter that does not prepend time/name/level information to messages,
 * because those fields are logged separately when using the Android logger.
 */
class AndroidFormatter extends Log.BasicFormatter {
  format(message) {
    return this.formatText(message);
  }
}

/*
 * AndroidAppender
 * Logs to Android logcat using AndroidLog.jsm
 */
class AndroidAppender extends Log.Appender {
  constructor(aFormatter) {
    super(aFormatter || new AndroidFormatter());
    this._name = "AndroidAppender";

    // Map log level to AndroidLog.foo method.
    this._mapping = {
      [Log.Level.Fatal]: "e",
      [Log.Level.Error]: "e",
      [Log.Level.Warn]: "w",
      [Log.Level.Info]: "i",
      [Log.Level.Config]: "d",
      [Log.Level.Debug]: "d",
      [Log.Level.Trace]: "v",
    };
  }

  append(aMessage) {
    if (!aMessage) {
      return;
    }

    // AndroidLog.jsm always prepends "Gecko" to the tag, so we strip any
    // leading "Gecko" here. Also strip dots to save space.
    const tag = aMessage.loggerName.replace(/^Gecko|\./g, "");
    const msg = this._formatter.format(aMessage);
    AndroidLog[this._mapping[aMessage.level]](tag, msg);
  }
}

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
  addLazyGetter(
    scope,
    name,
    { service, module, handler, observers, ppmm, mm, ged, init, once }
  ) {
    XPCOMUtils.defineLazyGetter(scope, name, _ => {
      let ret = undefined;
      if (module) {
        ret = ChromeUtils.import(module, {})[name];
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
      const observer = (subject, topic, data) => {
        Services.obs.removeObserver(observer, topic);
        if (!once) {
          Services.obs.addObserver(scope[name], topic);
        }
        scope[name].observe(subject, topic, data); // Explicitly notify new observer
      };
      observers.forEach(topic => Services.obs.addObserver(observer, topic));
    }

    if (!this.IS_PARENT_PROCESS) {
      // ppmm, mm, and ged are only available in the parent process.
      return;
    }

    const addMMListener = (target, names) => {
      const listener = msg => {
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
      const listener = (event, data, callback) => {
        EventDispatcher.instance.unregisterListener(listener, event);
        if (!once) {
          EventDispatcher.instance.registerListener(scope[name], event);
        }
        scope[name].onEvent(event, data, callback);
      };
      EventDispatcher.instance.registerListener(listener, ged);
    }
  },

  _addLazyListeners(events, handler, scope, name, addFn, handleFn) {
    if (!handler) {
      handler = _ =>
        Array.isArray(name) ? name.map(n => scope[n]) : scope[name];
    }
    const listener = (...args) => {
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
  addLazyEventListener(target, events, { handler, scope, name, options }) {
    this._addLazyListeners(
      events,
      handler,
      scope,
      name,
      (events, listener) => {
        events.forEach(event =>
          target.addEventListener(event, listener, options)
        );
      },
      (handlers, listener, args) => {
        if (!options || !options.once) {
          target.removeEventListener(args[0].type, listener, options);
          handlers.forEach(handler =>
            target.addEventListener(args[0].type, handler, options)
          );
        }
        handlers.forEach(handler => handler.handleEvent(args[0]));
      }
    );
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
  registerLazyWindowEventListener(
    window,
    events,
    { handler, scope, name, once }
  ) {
    const dispatcher = this.getDispatcherForWindow(window);

    this._addLazyListeners(
      events,
      handler,
      scope,
      name,
      (events, listener) => {
        dispatcher.registerListener(listener, events);
      },
      (handlers, listener, args) => {
        if (!once) {
          dispatcher.unregisterListener(listener, args[0]);
          handlers.forEach(handler =>
            dispatcher.registerListener(handler, args[0])
          );
        }
        handlers.forEach(handler => handler.onEvent(...args));
      }
    );
  },

  /**
   * Add lazy pref observers, and only load the actual handler once the pref
   * value changes from default, and every time the pref value changes
   * afterwards.
   *
   * @param aPrefs  Prefs as an object or array. Each pref object has fields
   *                "name" and "default", indicating the name and default value
   *                of the pref, respectively.
   * @param handler If specified, function that, for a given pref, returns the
   *                actual event handler as an object or an array of objects.
   *                If handler is not specified, the actual event handler is
   *                specified using the scope and name pair.
   * @param scope   See handler.
   * @param name    See handler.
   * @param once    If true, only observe the specified prefs once.
   */
  addLazyPrefObserver(aPrefs, { handler, scope, name, once }) {
    this._addLazyListeners(
      aPrefs,
      handler,
      scope,
      name,
      (prefs, observer) => {
        prefs.forEach(pref => Services.prefs.addObserver(pref.name, observer));
        prefs.forEach(pref => {
          if (pref.default === undefined) {
            return;
          }
          let value;
          switch (typeof pref.default) {
            case "string":
              value = Services.prefs.getCharPref(pref.name, pref.default);
              break;
            case "number":
              value = Services.prefs.getIntPref(pref.name, pref.default);
              break;
            case "boolean":
              value = Services.prefs.getBoolPref(pref.name, pref.default);
              break;
          }
          if (pref.default !== value) {
            // Notify observer if value already changed from default.
            observer(Services.prefs, "nsPref:changed", pref.name);
          }
        });
      },
      (handlers, observer, args) => {
        if (!once) {
          Services.prefs.removeObserver(args[2], observer);
          handlers.forEach(handler =>
            Services.prefs.addObserver(args[2], observer)
          );
        }
        handlers.forEach(handler => handler.observe(...args));
      }
    );
  },

  getRootDocShell(aWin) {
    if (!aWin) {
      return null;
    }
    let docShell;
    try {
      docShell = aWin.QueryInterface(Ci.nsIDocShell);
    } catch (e) {
      docShell = aWin.docShell;
    }
    return docShell.rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor);
  },

  /**
   * Return the outermost chrome DOM window (the XUL window) for a given DOM
   * window, in the parent process.
   *
   * @param aWin a DOM window.
   */
  getChromeWindow(aWin) {
    const docShell = this.getRootDocShell(aWin);
    return docShell && docShell.domWindow;
  },

  /**
   * Return the content frame message manager (aka the frame script global
   * object) for a given DOM window, in a child process.
   *
   * @param aWin a DOM window.
   */
  getContentFrameMessageManager(aWin) {
    const docShell = this.getRootDocShell(aWin);
    return docShell && docShell.getInterface(Ci.nsIBrowserChild).messageManager;
  },

  /**
   * Return the per-nsWindow EventDispatcher for a given DOM window, in either
   * the parent process or a child process.
   *
   * @param aWin a DOM window.
   */
  getDispatcherForWindow(aWin) {
    try {
      if (!this.IS_PARENT_PROCESS) {
        const mm = this.getContentFrameMessageManager(aWin.top || aWin);
        return mm && EventDispatcher.forMessageManager(mm);
      }
      const win = this.getChromeWindow(aWin.top || aWin);
      if (!win.closed) {
        return win.WindowEventDispatcher || EventDispatcher.for(win);
      }
    } catch (e) {}
    return null;
  },

  getActiveDispatcherAndWindow() {
    const bc = Services.focus.activeBrowsingContext;
    const win = bc ? bc.window : null; // WON'T WORK FOR OOP IFRAMES!
    let dispatcher = this.getDispatcherForWindow(win);
    if (dispatcher) {
      return [dispatcher, win];
    }

    for (const win of Services.wm.getEnumerator(/* windowType */ null)) {
      dispatcher = this.getDispatcherForWindow(win);
      if (dispatcher) {
        return [dispatcher, win];
      }
    }
    return [null, null];
  },

  /**
   * Add logging functions to the specified scope that forward to the given
   * Log.jsm logger. Currently "debug" and "warn" functions are supported. To
   * log something, call the function through a template literal:
   *
   *   function foo(bar, baz) {
   *     debug `hello world`;
   *     debug `foo called with ${bar} as bar`;
   *     warn `this is a warning for ${baz}`;
   *   }
   *
   * An inline format can also be used for logging:
   *
   *   let bar = 42;
   *   do_something(bar); // No log.
   *   do_something(debug.foo = bar); // Output "foo = 42" to the log.
   *
   * @param aTag Name of the Log.jsm logger to forward logs to.
   * @param aScope Scope to add the logging functions to.
   */
  initLogging(aTag, aScope) {
    aScope = aScope || {};
    const tag = "GeckoView." + aTag.replace(/^GeckoView\.?/, "");

    // Only provide two levels for simplicity.
    // For "info", use "debug" instead.
    // For "error", throw an actual JS error instead.
    for (const level of ["DEBUG", "WARN"]) {
      const log = (strings, ...exprs) =>
        this._log(log.logger, level, strings, exprs);

      XPCOMUtils.defineLazyGetter(log, "logger", _ => {
        const logger = Log.repository.getLogger(tag);
        logger.parent = this.rootLogger;
        return logger;
      });

      aScope[level.toLowerCase()] = new Proxy(log, {
        set: (obj, prop, value) => obj([prop + " = ", ""], value) || true,
      });
    }
    return aScope;
  },

  get rootLogger() {
    if (!this._rootLogger) {
      this._rootLogger = Log.repository.getLogger("GeckoView");
      this._rootLogger.addAppender(new AndroidAppender());
      this._rootLogger.manageLevelFromPref("geckoview.logging");
    }
    return this._rootLogger;
  },

  _log(aLogger, aLevel, aStrings, aExprs) {
    if (!Array.isArray(aStrings)) {
      const [, file, line] = new Error().stack.match(/.*\n.*\n.*@(.*):(\d+):/);
      throw Error(
        `Expecting template literal: ${aLevel} \`foo \${bar}\``,
        file,
        +line
      );
    }

    if (aLogger.level > Log.Level.Numbers[aLevel]) {
      // Log disabled.
      return;
    }

    // Do some GeckoView-specific formatting:
    // * Remove newlines so long log lines can be put into multiple lines:
    //   debug `foo=${foo}
    //          bar=${bar}`;
    const strs = Array.from(aStrings);
    const regex = /\n\s*/g;
    for (let i = 0; i < strs.length; i++) {
      strs[i] = strs[i].replace(regex, " ");
    }

    // * Heuristically format flags as hex.
    // * Heuristically format nsresult as string name or hex.
    for (let i = 0; i < aExprs.length; i++) {
      const expr = aExprs[i];
      switch (typeof expr) {
        case "number":
          if (expr > 0 && /\ba?[fF]lags?[\s=:]+$/.test(strs[i])) {
            // Likely a flag; display in hex.
            aExprs[i] = `0x${expr.toString(0x10)}`;
          } else if (expr >= 0 && /\b(a?[sS]tatus|rv)[\s=:]+$/.test(strs[i])) {
            // Likely an nsresult; display in name or hex.
            aExprs[i] = `0x${expr.toString(0x10)}`;
            for (const name in Cr) {
              if (expr === Cr[name]) {
                aExprs[i] = name;
                break;
              }
            }
          }
          break;
      }
    }

    aLogger[aLevel.toLowerCase()](strs, ...aExprs);
  },
};

XPCOMUtils.defineLazyGetter(
  GeckoViewUtils,
  "IS_PARENT_PROCESS",
  _ => Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT
);
