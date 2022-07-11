/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["TestWindowChild"];

var docShellThunks = new Map();

class TestWindowChild extends JSWindowActorChild {
  constructor() {
    super();
    this.sawActorCreated = false;

    try {
      void this.contentWindow;
    } catch (e) {
      this.uninitializedGetterError = e;
    }
  }

  actorCreated() {
    this.sawActorCreated = true;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "toChild":
        aMessage.data.toChild = true;
        this.sendAsyncMessage("toParent", aMessage.data);
        break;
      case "asyncAdd":
        let { a, b } = aMessage.data;
        return new Promise(resolve => {
          resolve({ result: a + b });
        });
      case "error":
        return Promise.reject(new SyntaxError(aMessage.data.message));
      case "exception":
        return Promise.reject(
          Components.Exception(aMessage.data.message, aMessage.data.result)
        );
      case "done":
        this.done(aMessage.data);
        break;
      case "noncloneReply":
        // Return a value which is non-cloneable, like a WindowProxy.
        return this.contentWindow;
      case "storeActor":
        docShellThunks.set(this.docShell, this);
        break;
      case "checkActor": {
        let actor = docShellThunks.get(this.docShell);
        docShellThunks.delete(this.docShell);

        let contentWindow;
        let error;
        try {
          contentWindow = actor.contentWindow;
        } catch (e) {
          error = e;
        }
        if (error) {
          return {
            status: "error",
            errorType: error.name,
          };
        }
        return {
          status: "success",
          valueIsNull: contentWindow === null,
        };
      }
    }

    return undefined;
  }

  handleEvent(aEvent) {
    this.sendAsyncMessage("event", { type: aEvent.type });
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "audio-playback":
        this.done({ subject, topic, data });
        break;
      default:
        this.lastObserved = { subject, topic, data };
        break;
    }
  }

  show() {
    return "TestWindowChild";
  }

  didDestroy() {
    Services.obs.notifyObservers(this, "test-js-window-actor-diddestroy", true);
  }
}
