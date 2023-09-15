/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class EventModule extends Module {
  destroy() {}

  interceptEvent(name, payload) {
    if (name === "event.testEventWithInterception") {
      return {
        ...payload,
        additionalInformation: "information added through interception",
      };
    }

    if (name === "event.testEventCancelableWithInterception") {
      if (payload.shouldCancel) {
        return null;
      }
      return payload;
    }

    return payload;
  }

  /**
   * Commands
   */

  testEmitWindowGlobalInRootEvent(params, destination) {
    this.emitEvent("event-from-window-global-in-root", {
      text: `windowglobal-in-root event for ${destination.id}`,
    });
  }
}

export const event = EventModule;
