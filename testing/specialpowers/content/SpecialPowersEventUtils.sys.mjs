/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Loads a stub copy of EventUtils.js which can be used by things like
 * content tasks without holding any direct references to windows.
 */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

export let EventUtils = { setTimeout, window: {}, _EU_Ci: Ci, _EU_Cc: Cc };

EventUtils.parent = EventUtils.window;

EventUtils.synthesizeClick = element =>
  new Promise(resolve => {
    element.addEventListener("click", () => resolve(), { once: true });

    EventUtils.synthesizeMouseAtCenter(
      element,
      { type: "mousedown", isSynthesized: false },
      element.ownerGlobal
    );
    EventUtils.synthesizeMouseAtCenter(
      element,
      { type: "mouseup", isSynthesized: false },
      element.ownerGlobal
    );
  });

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);
