/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
});
XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

const FRAME_ACTOR_CONFIG = {
  parent: {
    esModuleURI:
      "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameParent.sys.mjs",
  },
  child: {
    esModuleURI:
      "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameChild.sys.mjs",
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
export const MessageHandlerFrameActor = {
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
