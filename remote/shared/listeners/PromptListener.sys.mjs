/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  modal: "chrome://remote/content/shared/Prompt.sys.mjs",
});
/**
 * The PromptListener listens to the DialogObserver events.
 *
 * Example:
 * ```
 * const listener = new PromptListener();
 * listener.on("opened", onPromptOpened);
 * listener.startListening();
 *
 * const onPromptOpened = (eventName, data = {}) => {
 *   const { contentBrowser, prompt } = data;
 *   ...
 * };
 * ```
 *
 * @fires message
 *    The PromptListener emits "opened" events,
 *    with the following object as payload:
 *      - {XULBrowser} contentBrowser
 *            The <xul:browser> which hold the <var>prompt</var>.
 *      - {modal.Dialog} prompt
 *            Returns instance of the Dialog class.
 */
export class PromptListener {
  #observer;

  constructor() {
    lazy.EventEmitter.decorate(this);
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#observer) {
      return;
    }

    this.#observer = new lazy.modal.DialogObserver(() => {});
    this.#observer.add(this.#onEvent);
  }

  stopListening() {
    if (!this.#observer) {
      return;
    }

    this.#observer.cleanup();
    this.#observer = null;
  }

  #onEvent = async (action, prompt, contentBrowser) => {
    if (action === lazy.modal.ACTION_OPENED) {
      this.emit("opened", {
        contentBrowser,
        prompt,
      });
    }
  };
}
