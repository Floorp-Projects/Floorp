/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

export class GeckoViewPrompterChild extends GeckoViewActorChild {
  constructor() {
    super();
    this._prompts = new Map();
  }

  dismissPrompt(prompt) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:Prompt:Dismiss",
      id: prompt.id,
    });
    this.unregisterPrompt(prompt);
  }

  updatePrompt(message) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:Prompt:Update",
      prompt: message,
    });
  }

  unregisterPrompt(prompt) {
    this._prompts.delete(prompt.id);
    this.sendAsyncMessage("UnregisterPrompt", {
      id: prompt.id,
    });
  }

  prompt(prompt, message) {
    this._prompts.set(prompt.id, prompt);
    this.sendAsyncMessage("RegisterPrompt", {
      id: prompt.id,
      promptType: prompt.getPromptType(),
    });
    // We intentionally do not await here as we want to fire NotifyPromptShow
    // immediately rather than waiting until the user accepts/dismisses the
    // prompt.
    const result = this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:Prompt",
      prompt: message,
    });
    this.sendAsyncMessage("NotifyPromptShow", {
      id: prompt.id,
    });
    return result;
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

const { debug, warn } = GeckoViewPrompterChild.initLogging("Prompter");
