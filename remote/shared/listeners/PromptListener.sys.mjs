/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  modal: "chrome://remote/content/shared/Prompt.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

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
 *
 *    The PromptListener emits "closed" events,
 *    with the following object as payload:
 *      - {XULBrowser} contentBrowser
 *            The <xul:browser> which is the target of the event.
 *      - {object} detail
 *        {boolean=} detail.accepted
 *            Returns true if a user prompt was accepted
 *            and false if it was dismissed.
 *        {string=} detail.userText
 *            The user text specified in a prompt.
 */
export class PromptListener {
  #curBrowserFn;
  #listening;

  constructor(curBrowserFn) {
    lazy.EventEmitter.decorate(this);

    // curBrowserFn is used only for Marionette (WebDriver classic).
    this.#curBrowserFn = curBrowserFn;
    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  /**
   * Waits for the prompt to be closed.
   *
   * @returns {Promise}
   *    Promise that resolves when the prompt is closed.
   */
  async dialogClosed() {
    return new Promise(resolve => {
      const dialogClosed = () => {
        this.off("closed", dialogClosed);
        resolve();
      };

      this.on("closed", dialogClosed);
    });
  }

  /**
   * Handles `DOMModalDialogClosed` events.
   */
  handleEvent(event) {
    lazy.logger.trace(`Received event ${event.type}`);

    const chromeWin = event.target.opener
      ? event.target.opener.ownerGlobal
      : event.target.ownerGlobal;
    const curBrowser = this.#curBrowserFn && this.#curBrowserFn();

    // For Marionette (WebDriver classic) we only care about events which come
    // the currently selected browser.
    if (curBrowser && chromeWin != curBrowser.window) {
      return;
    }

    let contentBrowser;
    if (lazy.AppInfo.isAndroid) {
      const tabBrowser = lazy.TabManager.getTabBrowser(event.target);
      // Since on Android we always have only one tab we can just check
      // the selected tab.
      const tab = tabBrowser.selectedTab;
      contentBrowser = lazy.TabManager.getBrowserForTab(tab);
    } else {
      contentBrowser = event.target;
    }

    const detail = {};

    // At the moment the event details are present for GeckoView and on desktop
    // only for Services.prompt.MODAL_TYPE_CONTENT prompts.
    if (event.detail) {
      const { areLeaving, value } = event.detail;
      // `areLeaving` returns undefined for alerts, for confirms and prompts
      // it returns true if a user prompt was accepted and false if it was dismissed.
      detail.accepted = areLeaving === undefined ? true : areLeaving;
      if (value) {
        detail.userText = value;
      }
    }

    this.emit("closed", {
      contentBrowser,
      detail,
    });
  }

  /**
   * Observes the following notifications:
   * `common-dialog-loaded` - when a modal dialog loaded on desktop,
   * `domwindowopened` - when a new chrome window opened,
   * `geckoview-prompt-show` - when a modal dialog opened on Android.
   */
  observe(subject, topic) {
    lazy.logger.trace(`Received observer notification ${topic}`);

    let curBrowser = this.#curBrowserFn && this.#curBrowserFn();
    switch (topic) {
      case "common-dialog-loaded":
        if (curBrowser) {
          if (
            !this.#hasCommonDialog(
              curBrowser.contentBrowser,
              curBrowser.window,
              subject
            )
          ) {
            return;
          }
        } else {
          const chromeWin = subject.opener
            ? subject.opener.ownerGlobal
            : subject.ownerGlobal;

          for (const tab of lazy.TabManager.getTabsForWindow(chromeWin)) {
            const contentBrowser = lazy.TabManager.getBrowserForTab(tab);
            const window = lazy.TabManager.getWindowForTab(tab);

            if (this.#hasCommonDialog(contentBrowser, window, subject)) {
              curBrowser = {
                contentBrowser,
                window,
              };

              break;
            }
          }
        }
        this.emit("opened", {
          contentBrowser: curBrowser.contentBrowser,
          prompt: new lazy.modal.Dialog(() => curBrowser, subject),
        });

        break;

      case "domwindowopened":
        subject.addEventListener("DOMModalDialogClosed", this);
        break;

      case "geckoview-prompt-show":
        for (let win of Services.wm.getEnumerator(null)) {
          const prompt = win.prompts().find(item => item.id == subject.id);
          if (prompt) {
            const tabBrowser = lazy.TabManager.getTabBrowser(win);
            // Since on Android we always have only one tab we can just check
            // the selected tab.
            const tab = tabBrowser.selectedTab;
            const contentBrowser = lazy.TabManager.getBrowserForTab(tab);
            const window = lazy.TabManager.getWindowForTab(tab);

            // Do not send the event if the curBrowser is specified,
            // and it's different from prompt browser.
            if (curBrowser && contentBrowser !== curBrowser.contentBrowser) {
              continue;
            }

            this.emit("opened", {
              contentBrowser,
              prompt: new lazy.modal.Dialog(
                () => ({
                  contentBrowser,
                  window,
                }),
                prompt
              ),
            });
            return;
          }
        }
        break;
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    this.#register();
    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    this.#unregister();
    this.#listening = false;
  }

  #hasCommonDialog(contentBrowser, window, prompt) {
    const modalType = prompt.Dialog.args.modalType;
    if (
      modalType === Services.prompt.MODAL_TYPE_TAB ||
      modalType === Services.prompt.MODAL_TYPE_CONTENT
    ) {
      // Find the container of the dialog in the parent document, and ensure
      // it is a descendant of the same container as the content browser.
      const container = contentBrowser.closest(".browserSidebarContainer");

      return container.contains(prompt.docShell.chromeEventHandler);
    }

    return prompt.ownerGlobal == window || prompt.opener?.ownerGlobal == window;
  }

  #register() {
    Services.obs.addObserver(this, "common-dialog-loaded");
    Services.obs.addObserver(this, "domwindowopened");
    Services.obs.addObserver(this, "geckoview-prompt-show");

    // Register event listener and save already open prompts for all already open windows.
    for (const win of Services.wm.getEnumerator(null)) {
      win.addEventListener("DOMModalDialogClosed", this);
    }
  }

  #unregister() {
    const removeObserver = observerName => {
      try {
        Services.obs.removeObserver(this, observerName);
      } catch (e) {
        lazy.logger.debug(`Failed to remove observer "${observerName}"`);
      }
    };

    for (const observerName of [
      "common-dialog-loaded",
      "domwindowopened",
      "geckoview-prompt-show",
    ]) {
      removeObserver(observerName);
    }

    // Unregister event listener for all open windows
    for (const win of Services.wm.getEnumerator(null)) {
      win.removeEventListener("DOMModalDialogClosed", this);
    }
  }
}
