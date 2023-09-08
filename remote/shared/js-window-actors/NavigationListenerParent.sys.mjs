/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  notifyLocationChanged:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStopped:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class NavigationListenerParent extends JSWindowActorParent {
  async receiveMessage(message) {
    try {
      switch (message.name) {
        case "NavigationListenerChild:locationChanged": {
          lazy.notifyLocationChanged({
            contextDetails: message.data.contextDetails,
            url: message.data.url,
          });
          break;
        }
        case "NavigationListenerChild:navigationStarted": {
          lazy.notifyNavigationStarted({
            contextDetails: message.data.contextDetails,
            url: message.data.url,
          });
          break;
        }
        case "NavigationListenerChild:navigationStopped": {
          lazy.notifyNavigationStopped({
            contextDetails: message.data.contextDetails,
            url: message.data.url,
          });
          break;
        }
        default:
          throw new Error("Unsupported message:" + message.name);
      }
    } catch (e) {
      if (e instanceof TypeError) {
        // Avoid error spam from errors due to unavailable browsing contexts.
        lazy.logger.trace(
          `Failed to handle a navigation listener message: ${e.message}`
        );
      } else {
        throw e;
      }
    }
  }
}
