/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon */

const {
  Management: {
    global: { windowTracker },
  },
} = ChromeUtils.import("resource://gre/modules/Extension.jsm", null);

function getNativeWindow() {
  return windowTracker.topWindow.NativeWindow;
}

const clickHandlers = new ExtensionCommon.EventEmitter();

const menuItem = getNativeWindow().menu.add({
  name: "Report site issue",
  callback: () => {
    clickHandlers.emit("click");
  },
  enabled: false,
  visible: false,
});

this.nativeMenu = class extends ExtensionAPI {
  getAPI(context) {
    return {
      nativeMenu: {
        onClicked: new ExtensionCommon.EventManager({
          context,
          name: "nativeMenu.onClicked",
          register: fire => {
            const callback = () => {
              fire.async().catch(() => {}); // ignore Message Manager disconnects
            };
            clickHandlers.on("click", callback);
            return () => {
              clickHandlers.off("click", callback);
            };
          },
        }).api(),
        async disable() {
          getNativeWindow().menu.update(menuItem, { enabled: false });
        },
        async enable() {
          getNativeWindow().menu.update(menuItem, { enabled: true });
        },
        async hide() {
          getNativeWindow().menu.update(menuItem, { visible: false });
        },
        async show() {
          getNativeWindow().menu.update(menuItem, { visible: true });
        },
        async setLabel(label) {
          getNativeWindow().menu.update(menuItem, { name: label });
        },
      },
    };
  }
};
