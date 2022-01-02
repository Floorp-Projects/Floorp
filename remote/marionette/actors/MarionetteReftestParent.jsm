/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = ["MarionetteReftestParent"];

/**
 * Parent JSWindowActor to handle navigation for reftests relying on marionette.
 */
class MarionetteReftestParent extends JSWindowActorParent {
  /**
   * Wait for the expected URL to be loaded.
   *
   * @param {String} url
   *        The expected url.
   * @param {Boolean} useRemote
   *        True if tests are running with e10s.
   * @return {Boolean} true if the page is fully loaded with the expected url,
   *         false otherwise.
   */
  async reftestWait(url, useRemote) {
    try {
      const isCorrectUrl = await this.sendQuery(
        "MarionetteReftestParent:reftestWait",
        {
          url,
          useRemote,
        }
      );

      if (isCorrectUrl) {
        // Trigger flush rendering for all remote frames.
        await this._flushRenderingInSubtree({
          ignoreThrottledAnimations: false,
        });
      }

      return isCorrectUrl;
    } catch (e) {
      if (e.name === "AbortError") {
        // If the query is aborted, the window global is being destroyed, most
        // likely because a navigation happened.
        return false;
      }

      // Other errors should not be swallowed.
      throw e;
    }
  }

  /**
   * Call flushRendering on all browsing contexts in the subtree.
   * Each actor will flush rendering in all the same process frames.
   */
  async _flushRenderingInSubtree({ ignoreThrottledAnimations }) {
    const browsingContext = this.manager.browsingContext;
    const contexts = browsingContext.getAllBrowsingContextsInSubtree();

    await Promise.all(
      contexts.map(async context => {
        if (context === browsingContext) {
          // Skip the top browsing context, for which flushRendering is
          // already performed via the initial reftestWait call.
          return;
        }

        const windowGlobal = context.currentWindowGlobal;
        if (!windowGlobal) {
          // Bail out if there is no window attached to the current context.
          return;
        }

        if (!windowGlobal.isProcessRoot) {
          // Bail out if this window global is not a process root.
          // MarionetteReftestChild::flushRendering will flush all same process
          // frames, so we only need to call flushRendering on process roots.
          return;
        }

        const reftestActor = windowGlobal.getActor("MarionetteReftest");
        await reftestActor.sendQuery("MarionetteReftestParent:flushRendering", {
          ignoreThrottledAnimations,
        });
      })
    );
  }
}
