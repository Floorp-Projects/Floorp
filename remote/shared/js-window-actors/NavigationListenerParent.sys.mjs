/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  notifyLocationChanged:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStopped:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
});

export class NavigationListenerParent extends JSWindowActorParent {
  async receiveMessage(message) {
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
  }
}
