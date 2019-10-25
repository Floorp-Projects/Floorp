/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTestUtilsChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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

      case "BrowserTestUtils:CrashFrame": {
        // This is to intentionally crash the frame.
        // We crash by using js-ctypes and dereferencing
        // a bad pointer. The crash should happen immediately
        // upon loading this frame script.

        const { ctypes } = ChromeUtils.import(
          "resource://gre/modules/ctypes.jsm"
        );

        let dies = function() {
          ChromeUtils.privateNoteIntentionalCrash();
          let zero = new ctypes.intptr_t(8);
          let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
          badptr.contents;
        };

        dump("\nEt tu, Brute?\n");
        dies();
      }
    }

    return undefined;
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
          let frame = cur.ownerGlobal.frameElement;
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
