/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = ["MarionetteReftestParent"];

/**
 * Parent JSWindowActor to handle navigation for reftests relying on marionette.
 */
class MarionetteReftestParent extends JSWindowActorParent {
  actorCreated() {
    logger.trace(`[${this.browsingContext.id}] Reftest Parent actor created`);
  }

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
}
