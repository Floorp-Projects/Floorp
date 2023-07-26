/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.sys.mjs",

  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

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
      pagehide: {},
      pageshow: {},
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
