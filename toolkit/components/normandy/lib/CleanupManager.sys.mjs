/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
});

class CleanupManagerClass {
  constructor() {
    this.handlers = new Set();
    this.cleanupPromise = null;
  }

  addCleanupHandler(handler) {
    this.handlers.add(handler);
  }

  removeCleanupHandler(handler) {
    this.handlers.delete(handler);
  }

  async cleanup() {
    if (this.cleanupPromise === null) {
      this.cleanupPromise = (async () => {
        for (const handler of this.handlers) {
          try {
            await handler();
          } catch (ex) {
            console.error(ex);
          }
        }
      })();

      // Block shutdown to ensure any cleanup tasks that write data are
      // finished.
      lazy.AsyncShutdown.profileBeforeChange.addBlocker(
        "ShieldRecipeClient: Cleaning up",
        this.cleanupPromise
      );
    }

    return this.cleanupPromise;
  }
}

export var CleanupManager = new CleanupManagerClass();
