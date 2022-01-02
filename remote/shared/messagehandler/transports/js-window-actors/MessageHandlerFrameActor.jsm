/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageHandlerFrameActor"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
});
XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const FRAME_ACTOR_CONFIG = {
  parent: {
    moduleURI:
      "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameParent.jsm",
  },
  child: {
    moduleURI:
      "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameChild.jsm",
    events: {
      DOMWindowCreated: {},
    },
  },
  allFrames: true,
};

/**
 * MessageHandlerFrameActor exposes a simple registration helper to lazily
 * register MessageHandlerFrame JSWindow actors.
 */
const MessageHandlerFrameActor = {
  registered: false,

  register: () => {
    if (this.registered) {
      return;
    }

    ActorManagerParent.addJSWindowActors({
      MessageHandlerFrame: FRAME_ACTOR_CONFIG,
    });
    this.registered = true;
    logger.trace("Registered MessageHandlerFrame actors");
  },
};
