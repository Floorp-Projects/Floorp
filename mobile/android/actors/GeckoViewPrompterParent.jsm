/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrompterParent"];

const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class GeckoViewPrompterParent extends GeckoViewActorParent {
  constructor() {
    super();
    this._prompts = new Map();
  }

  registerPrompt(promptId, getPromptType) {
    return this._prompts.set(promptId, getPromptType);
  }

  unregisterPrompt(promptId) {
    this._prompts.delete(promptId);
  }

  notifyPromptShow(promptId) {
    Services.obs.notifyObservers({ id: promptId }, "geckoview-prompt-show");
  }

  getPrompts() {
    const self = this;
    const prompts = [];
    // Marionette expects this event to be fired from the parent
    const dialogClosedEvent = new CustomEvent("DOMModalDialogClosed", {
      cancelable: true,
      bubbles: true,
    });
    for (const [promptId, promptType] of this._prompts) {
      prompts.push({
        args: {
          modalType: "GeckoViewPrompter",
          promptType,
        },
        setInputText(text) {
          self.setInputText(promptId, text);
        },
        async getPromptText() {
          return self.getPromptText(promptId);
        },
        async getInputText() {
          return self.getInputText(promptId);
        },
        acceptPrompt() {
          self.acceptPrompt(promptId);
          self.window.dispatchEvent(dialogClosedEvent);
        },
        dismissPrompt() {
          self.dismissPrompt(promptId);
          self.window.dispatchEvent(dialogClosedEvent);
        },
      });
    }
    return prompts;
  }

  /**
   * Handles the message coming from GeckoViewPrompterChild.
   *
   * @param   {string} message.name The subject of the message.
   * @param   {object} message.data The data of the message.
   */
  async receiveMessage({ name, data }) {
    switch (name) {
      case "RegisterPrompt": {
        this.registerPrompt(data.id, data.promptType);
        break;
      }
      case "UnregisterPrompt": {
        this.unregisterPrompt(data.id);
        break;
      }
      case "NotifyPromptShow": {
        this.notifyPromptShow(data.id);
        break;
      }
      default: {
        super.receiveMessage({ name, data });
        break;
      }
    }
  }

  getPromptType(promptId) {
    return this.sendQuery("GetPromptType", {
      id: promptId,
    });
  }

  getPromptText(promptId) {
    return this.sendQuery("GetPromptText", {
      id: promptId,
    });
  }

  getInputText(promptId) {
    return this.sendQuery("GetInputText", {
      id: promptId,
    });
  }

  setInputText(promptId, inputText) {
    this.sendAsyncMessage("SetInputText", {
      id: promptId,
      text: inputText,
    });
  }

  acceptPrompt(promptId) {
    this.sendAsyncMessage("AcceptPrompt", {
      id: promptId,
    });
  }

  dismissPrompt(promptId) {
    this.sendAsyncMessage("DismissPrompt", {
      id: promptId,
    });
  }
}
