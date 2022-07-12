/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageHandlerFrameActor"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
});
XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

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
  messageManagerGroups: ["browsers"],
};

/**
 * MessageHandlerFrameActor exposes a simple registration helper to lazily
 * register MessageHandlerFrame JSWindow actors.
 */
const MessageHandlerFrameActor = {
  registered: false,

  register() {
    if (this.registered) {
      return;
    }

    lazy.ActorManagerParent.addJSWindowActors({
      MessageHandlerFrame: FRAME_ACTOR_CONFIG,
    });
    this.registered = true;
    lazy.logger.trace("Registered MessageHandlerFrame actors");
  },
};
