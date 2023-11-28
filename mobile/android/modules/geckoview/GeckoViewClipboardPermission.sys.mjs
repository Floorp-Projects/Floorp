/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PromptUtils: "resource://gre/modules/PromptUtils.sys.mjs",
});

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const { debug } = GeckoViewUtils.initLogging("GeckoViewClipboardPermission");

export var GeckoViewClipboardPermission = {
  confirmUserPaste(aWindowContext) {
    return new Promise((resolve, reject) => {
      if (!aWindowContext) {
        reject(
          Components.Exception("Null window context.", Cr.NS_ERROR_INVALID_ARG)
        );
        return;
      }

      const { document } = aWindowContext.browsingContext.topChromeWindow;
      if (!document) {
        reject(
          Components.Exception(
            "Unable to get chrome document.",
            Cr.NS_ERROR_FAILURE
          )
        );
        return;
      }

      if (this._pendingRequest) {
        reject(
          Components.Exception(
            "There is an ongoing request.",
            Cr.NS_ERROR_FAILURE
          )
        );
        return;
      }

      this._pendingRequest = { resolve, reject };

      const mouseXInCSSPixels = {};
      const mouseYInCSSPixels = {};
      const windowUtils = document.ownerGlobal.windowUtils;
      windowUtils.getLastOverWindowPointerLocationInCSSPixels(
        mouseXInCSSPixels,
        mouseYInCSSPixels
      );
      const screenRect = windowUtils.toScreenRect(
        mouseXInCSSPixels.value,
        mouseYInCSSPixels.value,
        0,
        0
      );

      debug`confirmUserPaste (${screenRect.x}, ${screenRect.y})`;

      document.ownerGlobal.WindowEventDispatcher.sendRequestForResult({
        type: "GeckoView:ClipboardPermissionRequest",
        screenPoint: {
          x: screenRect.x,
          y: screenRect.y,
        },
      }).then(
        allowOrDeny => {
          const propBag = lazy.PromptUtils.objectToPropBag({ ok: allowOrDeny });
          this._pendingRequest.resolve(propBag);
          this._pendingRequest = null;
        },
        error => {
          debug`Permission error: ${error}`;
          this._pendingRequest.reject();
          this._pendingRequest = null;
        }
      );
    });
  },
};
