/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Input"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);

class Input extends Domain {
  // commands

  /**
   * Simulate key events.
   *
   * @param {Object} options
   *        - autoRepeat (not supported)
   *        - code (not supported)
   *        - key
   *        - isKeypad (not supported)
   *        - location (not supported)
   *        - modifiers
   *        - text (not supported)
   *        - type
   *        - unmodifiedTest (not supported)
   *        - windowsVirtualKeyCode
   */
  async dispatchKeyEvent(options) {
    const { key, modifiers, type, windowsVirtualKeyCode } = options;

    let domType;
    if (type == "keyDown" || type == "rawKeyDown") {
      // 'rawKeyDown' is passed as type by puppeteer for all non-text keydown events:
      // See https://github.com/GoogleChrome/puppeteer/blob/2d99d85976dcb28cc6e3bad4b6a00cd61a67a2cf/lib/Input.js#L52
      // For now we simply map rawKeyDown to keydown.
      domType = "keydown";
    } else if (type == "keyUp" || type == "char") {
      // 'char' is fired as a single key event. Behind the scenes it will trigger keydown,
      // keypress and keyup. `domType` will only be used as the event to wait for.
      domType = "keyup";
    } else {
      throw new Error(`Unknown key event type ${type}`);
    }

    const { browser } = this.session.target;
    const browserWindow = browser.ownerGlobal;

    const EventUtils = this._getEventUtils(browserWindow);
    const onEvent = new Promise(r => {
      browserWindow.addEventListener(domType, r, {
        mozSystemGroup: true,
        once: true,
      });
    });

    if (type == "char") {
      // type == "char" is used when doing `await page.keyboard.type( 'I’m a list' );`
      // the ’ character will be calling dispatchKeyEvent only once with type=char.
      EventUtils.synthesizeKey(key, {}, browserWindow);
    } else {
      // Non printable keys should be prefixed with `KEY_`
      const eventUtilsKey = key.length == 1 ? key : "KEY_" + key;
      EventUtils.synthesizeKey(
        eventUtilsKey,
        {
          keyCode: windowsVirtualKeyCode,
          type: domType,
          altKey: !!(modifiers & 1),
          ctrlKey: !!(modifiers & 2),
          metaKey: !!(modifiers & 4),
          shiftKey: !!(modifiers & 8),
        },
        browserWindow
      );
    }
    await onEvent;
  }

  async dispatchMouseEvent({ type, button, x, y, modifiers, clickCount }) {
    if (type == "mousePressed") {
      type = "mousedown";
    } else if (type == "mouseReleased") {
      type = "mouseup";
    } else if (type == "mouseMoved") {
      type = "mousemove";
    } else {
      throw new Error(`Mouse type is not supported: ${type}`);
    }

    if (type === "mousedown" && button === "right") {
      type = "contextmenu";
    }
    if (button == undefined || button == "none" || button == "left") {
      button = 0;
    } else if (button == "middle") {
      button = 1;
    } else if (button == "right") {
      button = 2;
    } else if (button == "back") {
      button = 3;
    } else if (button == "forward") {
      button = 4;
    } else {
      throw new Error(`Mouse button is not supported: ${button}`);
    }

    // Gutenberg test packages/e2e-tests/specs/blocks/list.test.js:
    // "can be created by converting multiple paragraphs"
    // Works better with EventUtils, in order to make the Shift+Click to work
    const { browser } = this.session.target;
    const currentWindow = browser.ownerGlobal;
    const EventUtils = this._getEventUtils(currentWindow);
    EventUtils.synthesizeMouse(browser, x, y, {
      type,
      button,
      clickCount: clickCount || 1,
      altKey: !!(modifiers & 1),
      ctrlKey: !!(modifiers & 2),
      metaKey: !!(modifiers & 4),
      shiftKey: !!(modifiers & 8),
    });
  }

  /**
   * Memoized EventUtils getter.
   */
  _getEventUtils(win) {
    if (!this._eventUtils) {
      this._eventUtils = {
        window: win,
        parent: win,
        _EU_Ci: Ci,
        _EU_Cc: Cc,
      };
      Services.scriptloader.loadSubScript(
        "chrome://remote/content/external/EventUtils.js",
        this._eventUtils
      );
    }
    return this._eventUtils;
  }
}
