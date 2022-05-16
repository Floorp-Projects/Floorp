/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrompterChild"];

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

class GeckoViewPrompterChild extends GeckoViewActorChild {
  constructor() {
    super();
    this._prompts = new Map();
  }

  registerPrompt(prompt) {
    this._prompts.set(prompt.id, prompt);
    this.sendAsyncMessage("RegisterPrompt", {
      id: prompt.id,
      promptType: prompt.getPromptType(),
    });
  }

  unregisterPrompt(prompt) {
    this._prompts.delete(prompt.id);
    this.sendAsyncMessage("UnregisterPrompt", {
      id: prompt.id,
    });
  }

  notifyPromptShow(prompt) {
    this.sendAsyncMessage("NotifyPromptShow", {
      id: prompt.id,
    });
  }

  /**
   * Handles the message coming from GeckoViewPrompterParent.
   *
   * @param   {string} message.name The subject of the message.
   * @param   {object} message.data The data of the message.
   */
  async receiveMessage({ name, data }) {
    const prompt = this._prompts.get(data.id);
    if (!prompt) {
      // Unknown prompt, probably for a different child actor.
      return;
    }
    switch (name) {
      case "GetPromptText": {
        // eslint-disable-next-line consistent-return
        return prompt.getPromptText();
      }
      case "GetInputText": {
        // eslint-disable-next-line consistent-return
        return prompt.getInputText();
      }
      case "SetInputText": {
        prompt.setInputText(data.text);
        break;
      }
      case "AcceptPrompt": {
        prompt.accept();
        break;
      }
      case "DismissPrompt": {
        prompt.dismiss();
        break;
      }
      default: {
        break;
      }
    }
  }
}
