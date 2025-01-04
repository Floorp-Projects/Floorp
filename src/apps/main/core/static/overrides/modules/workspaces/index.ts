/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Workspaces from "@core/common/workspaces";

export const overrides = [
  () => {
    window.BrowserCommands.openTab = ({
      event,
      url,
    }: {
      event?: MouseEvent;
      url?: string;
    } = {}) => {
      const werePassedURL = !!url;
      url ??= window.BROWSER_NEW_TAB_URL;
      const searchClipboard =
        window.gMiddleClickNewTabUsesPasteboard && event?.button === 1;

      let relatedToCurrent = false;
      let where = "tab";
      const OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
        "floorp.browser.tabs.openNewTabPosition",
      );

      switch (OPEN_NEW_TAB_POSITION_PREF) {
        case 0:
          relatedToCurrent = false;
          break;
        case 1:
          relatedToCurrent = true;
          break;
        default:
          if (event) {
            where = window.BrowserUtils.whereToOpenLink(event, false, true);

            switch (where) {
              case "tab":
              case "tabshifted":
                // When accel-click or middle-click are used, open the new tab as
                // related to the current tab.
                relatedToCurrent = true;
                break;
              case "current":
                where = "tab";
                break;
            }
          }
      }

      // A notification intended to be useful for modular peformance tracking
      // starting as close as is reasonably possible to the time when the user
      // expressed the intent to open a new tab.  Since there are a lot of
      // entry points, this won't catch every single tab created, but most
      // initiated by the user should go through here.
      //
      // Note 1: This notification gets notified with a promise that resolves
      //         with the linked browser when the tab gets created
      // Note 2: This is also used to notify a user that an extension has changed
      //         the New Tab page.

      const gWorkspacesServices = Workspaces.ctx!;

      Services.obs.notifyObservers(
        {
          wrappedJSObject: new Promise((resolve) => {
            // biome-ignore lint/suspicious/noExplicitAny: <explanation>
            const options: any = {
              relatedToCurrent,
              resolveOnNewTabCreated: resolve,
              userContextId:
                gWorkspacesServices.getCurrentWorkspaceUserContextId,
            };
            if (!werePassedURL && searchClipboard) {
              let clipboard = window.readFromClipboard();
              clipboard =
                window.UrlbarUtils.stripUnsafeProtocolOnPaste(clipboard).trim();
              if (clipboard) {
                url = clipboard;
                options.allowThirdPartyFixup = true;
              }
            }
            window.openTrustedLinkIn(url, where, options);
          }),
        },
        "browser-open-newtab-start",
      );
    };
  },
];
