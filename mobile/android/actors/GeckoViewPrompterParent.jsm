/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrompterParent"];

const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

const DIALOGS = [
  "alert",
  "alertCheck",
  "confirm",
  "confirmCheck",
  "prompt",
  "promptCheck",
];

class GeckoViewPrompterParent extends GeckoViewActorParent {
  constructor() {
    super();
    this._prompts = new Map();
  }

  get rootActor() {
    return this.browsingContext.top.currentWindowGlobal.getActor(
      "GeckoViewPrompter"
    );
  }

  registerPrompt(promptId, promptType, actor) {
    return this._prompts.set(
      promptId,
      new RemotePrompt(promptId, promptType, actor)
    );
  }

  unregisterPrompt(promptId) {
    this._prompts.delete(promptId);
  }

  notifyPromptShow(promptId) {
    // ToDo: Bug 1761480 - GeckoView can send additional prompts to Marionette
    if (this._prompts.get(promptId).isDialog) {
      Services.obs.notifyObservers({ id: promptId }, "geckoview-prompt-show");
    }
  }

  getPrompts() {
    const self = this;
    const prompts = [];
    // Marionette expects this event to be fired from the parent
    const dialogClosedEvent = new CustomEvent("DOMModalDialogClosed", {
      cancelable: true,
      bubbles: true,
    });
    for (const [, prompt] of this._prompts) {
      // Adding only WebDriver compliant dialogs to the window
      if (prompt.isDialog) {
        prompts.push({
          args: {
            modalType: "GeckoViewPrompter",
            promptType: prompt.type,
            isDialog: prompt.isDialog,
          },
          setInputText(text) {
            prompt.setInputText(text);
          },
          async getPromptText() {
            return prompt.getPromptText();
          },
          async getInputText() {
            return prompt.getInputText();
          },
          acceptPrompt() {
            prompt.acceptPrompt();
            self.window.dispatchEvent(dialogClosedEvent);
          },
          dismissPrompt() {
            prompt.dismissPrompt();
            self.window.dispatchEvent(dialogClosedEvent);
          },
        });
      }
    }
    return prompts;
  }

  /**
   * Handles the message coming from GeckoViewPrompterChild.
   *
   * @param   {string} message.name The subject of the message.
   * @param   {object} message.data The data of the message.
   */
  // eslint-disable-next-line consistent-return
  async receiveMessage({ name, data }) {
    switch (name) {
      case "RegisterPrompt": {
        this.rootActor.registerPrompt(data.id, data.promptType, this);
        break;
      }
      case "UnregisterPrompt": {
        this.rootActor.unregisterPrompt(data.id);
        break;
      }
      case "NotifyPromptShow": {
        this.rootActor.notifyPromptShow(data.id);
        break;
      }
      default: {
        return super.receiveMessage({ name, data });
      }
    }
  }
}

class RemotePrompt {
  constructor(id, type, actor) {
    this.id = id;
    this.type = type;
    this.actor = actor;
  }

  // Checks if the prompt conforms to a WebDriver simple dialog.
  get isDialog() {
    return DIALOGS.includes(this.type);
  }

  getPromptText() {
    return this.actor.sendQuery("GetPromptText", {
      id: this.id,
    });
  }

  getInputText() {
    return this.actor.sendQuery("GetInputText", {
      id: this.id,
    });
  }

  setInputText(inputText) {
    this.actor.sendAsyncMessage("SetInputText", {
      id: this.id,
      text: inputText,
    });
  }

  acceptPrompt() {
    this.actor.sendAsyncMessage("AcceptPrompt", {
      id: this.id,
    });
  }

  dismissPrompt() {
    this.actor.sendAsyncMessage("DismissPrompt", {
      id: this.id,
    });
  }
}
