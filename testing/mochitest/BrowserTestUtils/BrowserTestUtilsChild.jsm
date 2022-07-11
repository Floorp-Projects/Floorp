/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTestUtilsChild"];

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);

class BrowserTestUtilsChildObserver {
  constructor() {
    this.currentObserverStatus = "";
    this.observerItems = [];
  }

  startObservingTopics(aTopics) {
    for (let topic of aTopics) {
      Services.obs.addObserver(this, topic);
      this.observerItems.push({ topic });
    }
  }

  stopObservingTopics(aTopics) {
    if (aTopics) {
      for (let topic of aTopics) {
        let index = this.observerItems.findIndex(item => item.topic == topic);
        if (index >= 0) {
          Services.obs.removeObserver(this, topic);
          this.observerItems.splice(index, 1);
        }
      }
    } else {
      for (let topic of this.observerItems) {
        Services.obs.removeObserver(this, topic);
      }
      this.observerItems = [];
    }

    if (this.currentObserverStatus) {
      let error = new Error(this.currentObserverStatus);
      this.currentObserverStatus = "";
      throw error;
    }
  }

  observeTopic(topic, count, filterFn, callbackResolver) {
    // If the topic is in the list already, assume that it came from a
    // startObservingTopics call. If it isn't in the list already, assume
    // that it isn't within a start/stop set and the observer has to be
    // removed afterwards.
    let removeObserver = false;
    let index = this.observerItems.findIndex(item => item.topic == topic);
    if (index == -1) {
      removeObserver = true;
      this.startObservingTopics([topic]);
    }

    for (let item of this.observerItems) {
      if (item.topic == topic) {
        item.count = count || 1;
        item.filterFn = filterFn;
        item.promiseResolver = () => {
          if (removeObserver) {
            this.stopObservingTopics([topic]);
          }
          callbackResolver();
        };
        break;
      }
    }
  }

  observe(aSubject, aTopic, aData) {
    for (let item of this.observerItems) {
      if (item.topic != aTopic) {
        continue;
      }
      if (item.filterFn && !item.filterFn(aSubject, aTopic, aData)) {
        break;
      }

      if (--item.count >= 0) {
        if (item.count == 0 && item.promiseResolver) {
          item.promiseResolver();
        }
        return;
      }
    }

    // Otherwise, if the observer doesn't match, fail.
    console.log(
      "Failed: Observer topic " + aTopic + " not expected in content process"
    );
    this.currentObserverStatus +=
      "Topic " + aTopic + " not expected in content process\n";
  }
}

BrowserTestUtilsChildObserver.prototype.QueryInterface = ChromeUtils.generateQI(
  ["nsIObserver", "nsISupportsWeakReference"]
);

class BrowserTestUtilsChild extends JSWindowActorChild {
  actorCreated() {
    this._EventUtils = null;
  }

  get EventUtils() {
    if (!this._EventUtils) {
      // Set up a dummy environment so that EventUtils works. We need to be careful to
      // pass a window object into each EventUtils method we call rather than having
      // it rely on the |window| global.
      let win = this.contentWindow;
      let EventUtils = {
        get KeyboardEvent() {
          return win.KeyboardEvent;
        },
        // EventUtils' `sendChar` function relies on the navigator to synthetize events.
        get navigator() {
          return win.navigator;
        },
      };

      EventUtils.window = {};
      EventUtils.parent = EventUtils.window;
      EventUtils._EU_Ci = Ci;
      EventUtils._EU_Cc = Cc;

      Services.scriptloader.loadSubScript(
        "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
        EventUtils
      );

      this._EventUtils = EventUtils;
    }

    return this._EventUtils;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Test:SynthesizeMouse": {
        return this.synthesizeMouse(aMessage.data, this.contentWindow);
      }

      case "Test:SynthesizeTouch": {
        return this.synthesizeTouch(aMessage.data, this.contentWindow);
      }

      case "Test:SendChar": {
        return this.EventUtils.sendChar(aMessage.data.char, this.contentWindow);
      }

      case "Test:SynthesizeKey":
        this.EventUtils.synthesizeKey(
          aMessage.data.key,
          aMessage.data.event || {},
          this.contentWindow
        );
        break;

      case "Test:SynthesizeComposition": {
        return this.EventUtils.synthesizeComposition(
          aMessage.data.event,
          this.contentWindow
        );
      }

      case "Test:SynthesizeCompositionChange":
        this.EventUtils.synthesizeCompositionChange(
          aMessage.data.event,
          this.contentWindow
        );
        break;

      case "BrowserTestUtils:StartObservingTopics": {
        this.observer = new BrowserTestUtilsChildObserver();
        this.observer.startObservingTopics(aMessage.data.topics);
        break;
      }

      case "BrowserTestUtils:StopObservingTopics": {
        if (this.observer) {
          this.observer.stopObservingTopics(aMessage.data.topics);
          this.observer = null;
        }
        break;
      }

      case "BrowserTestUtils:ObserveTopic": {
        return new Promise(resolve => {
          let filterFn;
          if (aMessage.data.filterFunctionSource) {
            /* eslint-disable-next-line no-eval */
            filterFn = eval(
              `(() => (${aMessage.data.filterFunctionSource}))()`
            );
          }

          let observer = this.observer || new BrowserTestUtilsChildObserver();
          observer.observeTopic(
            aMessage.data.topic,
            aMessage.data.count,
            filterFn,
            resolve
          );
        });
      }

      case "BrowserTestUtils:CrashFrame": {
        // This is to intentionally crash the frame.
        // We crash by using js-ctypes. The crash
        // should happen immediately
        // upon loading this frame script.

        const { ctypes } = ChromeUtils.import(
          "resource://gre/modules/ctypes.jsm"
        );

        let dies = function() {
          dump("\nEt tu, Brute?\n");
          ChromeUtils.privateNoteIntentionalCrash();

          switch (aMessage.data.crashType) {
            case "CRASH_OOM": {
              let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(
                Ci.nsIDebug2
              );
              debug.crashWithOOM();
              break;
            }
            case "CRASH_INVALID_POINTER_DEREF": // Fallthrough
            default: {
              // Dereference a bad pointer.
              let zero = new ctypes.intptr_t(8);
              let badptr = ctypes.cast(
                zero,
                ctypes.PointerType(ctypes.int32_t)
              );
              badptr.contents;
            }
          }
        };

        if (aMessage.data.asyncCrash) {
          let { setTimeout } = ChromeUtils.import(
            "resource://gre/modules/Timer.jsm"
          );
          // Get out of the stack.
          setTimeout(dies, 0);
        } else {
          dies();
        }
      }
    }

    return undefined;
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded":
      case "load": {
        this.sendAsyncMessage(aEvent.type, {
          internalURL: aEvent.target.documentURI,
          visibleURL: aEvent.target.location
            ? aEvent.target.location.href
            : null,
        });
        break;
      }
    }
  }

  synthesizeMouse(data, window) {
    let target = data.target;
    if (typeof target == "string") {
      target = this.document.querySelector(target);
    } else if (typeof data.targetFn == "string") {
      let runnablestr = `
        (() => {
          return (${data.targetFn});
        })();`;
      /* eslint-disable no-eval */
      target = eval(runnablestr)();
      /* eslint-enable no-eval */
    }

    let left = data.x;
    let top = data.y;
    if (target) {
      if (target.ownerDocument !== this.document) {
        // Account for nodes found in iframes.
        let cur = target;
        do {
          // eslint-disable-next-line mozilla/use-ownerGlobal
          let frame = cur.ownerDocument.defaultView.frameElement;
          let rect = frame.getBoundingClientRect();

          left += rect.left;
          top += rect.top;

          cur = frame;
        } while (cur && cur.ownerDocument !== this.document);

        // node must be in this document tree.
        if (!cur) {
          throw new Error("target must be in the main document tree");
        }
      }

      let rect = target.getBoundingClientRect();
      left += rect.left;
      top += rect.top;

      if (data.event.centered) {
        left += rect.width / 2;
        top += rect.height / 2;
      }
    }

    let result;

    lazy.E10SUtils.wrapHandlingUserInput(window, data.handlingUserInput, () => {
      if (data.event && data.event.wheel) {
        this.EventUtils.synthesizeWheelAtPoint(left, top, data.event, window);
      } else {
        result = this.EventUtils.synthesizeMouseAtPoint(
          left,
          top,
          data.event,
          window
        );
      }
    });

    return result;
  }

  synthesizeTouch(data, window) {
    let target = data.target;
    if (typeof target == "string") {
      target = this.document.querySelector(target);
    } else if (typeof data.targetFn == "string") {
      let runnablestr = `
        (() => {
          return (${data.targetFn});
        })();`;
      /* eslint-disable no-eval */
      target = eval(runnablestr)();
      /* eslint-enable no-eval */
    }

    if (target) {
      if (target.ownerDocument !== this.document) {
        // Account for nodes found in iframes.
        let cur = target;
        do {
          cur = cur.ownerGlobal.frameElement;
        } while (cur && cur.ownerDocument !== this.document);

        // node must be in this document tree.
        if (!cur) {
          throw new Error("target must be in the main document tree");
        }
      }
    }

    return this.EventUtils.synthesizeTouch(
      target,
      data.x,
      data.y,
      data.event,
      window
    );
  }
}
