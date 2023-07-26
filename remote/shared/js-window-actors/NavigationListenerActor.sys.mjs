/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

let registered = false;
export function isNavigationListenerActorRegistered() {
  return registered;
}

/**
 * Register the NavigationListener actor that will keep track of all ongoing
 * navigations.
 */
export function registerNavigationListenerActor() {
  if (registered) {
    return;
  }

  try {
    ChromeUtils.registerWindowActor("NavigationListener", {
      kind: "JSWindowActor",
      parent: {
        esModuleURI:
          "chrome://remote/content/shared/js-window-actors/NavigationListenerParent.sys.mjs",
      },
      child: {
        esModuleURI:
          "chrome://remote/content/shared/js-window-actors/NavigationListenerChild.sys.mjs",
        events: {
          DOMWindowCreated: {},
        },
      },
      allFrames: true,
      messageManagerGroups: ["browsers"],
    });
    registered = true;

    // Ensure the navigation listener is started in existing contexts.
    for (const browser of lazy.TabManager.browsers) {
      if (!browser?.browsingContext) {
        continue;
      }

      for (const context of browser.browsingContext.getAllBrowsingContextsInSubtree()) {
        context.currentWindowGlobal
          .getActor("NavigationListener")
          // Note that "createActor" is not explicitly referenced in the child
          // actor, this is only used to trigger the creation of the actor.
          .sendAsyncMessage("createActor");
      }
    }
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`NavigationListener actor is already registered!`);
    } else {
      throw e;
    }
  }
}

export function unregisterNavigationListenerActor() {
  if (!registered) {
    return;
  }
  ChromeUtils.unregisterWindowActor("NavigationListener");
  registered = false;
}
