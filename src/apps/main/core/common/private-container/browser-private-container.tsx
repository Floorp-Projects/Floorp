/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "@core/utils/context-menu";
import { PrivateContainer } from "./PrivateContainer";
import { createRootHMR } from "@nora/solid-xul";

export class FloorpPrivateContainer {

  private get privateContainerMenuItem() {
    return document.querySelector("#open_in_private_container") as XULElement;
  }
  private get openLinkMenuItem() {
    return document.querySelector("#context-openlink") as XULElement;
  }

  constructor() {
    // Create a private container.
    PrivateContainer.StartupCreatePrivateContainer();
    PrivateContainer.removePrivateContainerData();

    window.SessionStore.promiseInitialized.then(() => {
      window.gBrowser.tabContainer.addEventListener(
        "TabClose",
        FloorpPrivateContainer.removeDataIfPrivateContainerTabNotExist,
      );

      window.gBrowser.tabContainer.addEventListener(
        "TabOpen",
        FloorpPrivateContainer.handleTabModifications,
      );

      createRootHMR(()=>{
        // add URL link a context menu to open in private container.
        ContextMenuUtils.addContextBox(
          "open_in_private_container",
          "open-in_private-container",
          "context-openlink",
          () =>
            FloorpPrivateContainer.openWithPrivateContainer(
              window.gContextMenu.linkURL,
            ),
          "context-openlink",
          () => {
            this.privateContainerMenuItem.hidden = this.openLinkMenuItem.hidden;
          },
        );
      },import.meta.hot)
    });
  }

  public static checkPrivateContainerTabExist() {
    const privateContainer = PrivateContainer.getPrivateContainer();
    if (!privateContainer || !privateContainer.userContextId) {
      return false;
    }
    return window.gBrowser.tabs.some(
      (tab: { userContextId: number }) =>
        tab.userContextId === privateContainer.userContextId,
    );
  }

  private static removeDataIfPrivateContainerTabNotExist() {
    const privateContainerUserContextID =
      PrivateContainer.getPrivateContainerUserContextId();
    setTimeout(() => {
      if (!FloorpPrivateContainer.checkPrivateContainerTabExist()) {
        PrivateContainer.removePrivateContainerData();
      }

      return window.gBrowser.tabs.filter(
        (tab: { userContextId: number }) =>
          tab.userContextId === privateContainerUserContextID,
      );
    }, 400);
  }

  private static tabIsSaveHistory(tab: {
    getAttribute: (arg0: string) => string;
  }) {
    return tab.getAttribute("historydisabled") === "true";
  }

  private static applyDoNotSaveHistoryToTab(tab: {
    linkedBrowser: { setAttribute: (arg0: string, arg1: string) => void };
    setAttribute: (arg0: string, arg1: string) => void;
  }) {
    tab.linkedBrowser.setAttribute("disablehistory", "true");
    tab.linkedBrowser.setAttribute("disableglobalhistory", "true");
    tab.setAttribute("floorp-disablehistory", "true");
  }

  private static checkTabIsPrivateContainer(tab: { userContextId: number }) {
    const privateContainerUserContextID =
      PrivateContainer.getPrivateContainerUserContextId();
    return tab.userContextId === privateContainerUserContextID;
  }

  private static handleTabModifications() {
    const tabs = window.gBrowser.tabs;
    for (const tab of tabs) {
      if (
        FloorpPrivateContainer.checkTabIsPrivateContainer(tab) &&
        !FloorpPrivateContainer.tabIsSaveHistory(tab)
      ) {
        FloorpPrivateContainer.applyDoNotSaveHistoryToTab(tab);
      }
    }
  }

  public static openWithPrivateContainer(url: URI) {
    let relatedToCurrent = false; //"relatedToCurrent" decide where to open the new tab. Default work as last tab (right side). Floorp use this.
    const _OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
      "floorp.browser.tabs.openNewTabPosition",
      -1,
    );
    switch (_OPEN_NEW_TAB_POSITION_PREF) {
      case 0:
        // Open the new tab as unrelated to the current tab.
        relatedToCurrent = false;
        break;
      case 1:
        // Open the new tab as related to the current tab.
        relatedToCurrent = true;
        break;
    }
    const privateContainerUserContextID =
      PrivateContainer.getPrivateContainerUserContextId();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: new Promise((resolve) => {
          window.openTrustedLinkIn(url, "tab", {
            relatedToCurrent,
            resolveOnNewTabCreated: resolve,
            userContextId: privateContainerUserContextID,
          });
        }),
      } as nsISupports,
      "browser-open-newtab-start",
    );
  }

  public static reopenInPrivateContainer() {
    let userContextId = PrivateContainer.getPrivateContainerUserContextId();
    const reopenedTabs = window.TabContextMenu.contextTab.multiselected
      ? window.gBrowser.selectedTabs
      : [window.TabContextMenu.contextTab];

    for (const tab of reopenedTabs) {
      /* Create a triggering principal that is able to load the new tab
           For content principals that are about: chrome: or resource: we need system to load them.
           Anything other than system principal needs to have the new userContextId.
        */
      let triggeringPrincipal: nsIPrincipal;

      if (tab.linkedPanel) {
        triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
      } else {
        // For lazy tab browsers, get the original principal
        // from SessionStore
        const tabState = JSON.parse(window.SessionStore.getTabState(tab));
        try {
          triggeringPrincipal = window.E10SUtils.deserializePrincipal(
            tabState.triggeringPrincipal_base64,
          );
        } catch (ex) {
          continue;
        }
      }

      if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
        // Ensure that we have a null principal if we couldn't
        // deserialize it (for lazy tab browsers) ...
        // This won't always work however is safe to use.
        triggeringPrincipal =
          Services.scriptSecurityManager.createNullPrincipal({
            userContextId,
          });
      } else if (triggeringPrincipal.isContentPrincipal) {
        triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
          triggeringPrincipal,
          {
            userContextId,
          },
        );
      }

      const currentTabUserContextId = Number.parseInt(
        tab.getAttribute("usercontextid"),
      );

      if (currentTabUserContextId === userContextId) {
        userContextId = 0;
      }

      const newTab = window.gBrowser.addTab(tab.linkedBrowser.currentURI.spec, {
        userContextId,
        pinned: tab.pinned,
        index: tab._tPos + 1,
        triggeringPrincipal,
      });

      if (window.gBrowser.selectedTab === tab) {
        window.gBrowser.selectedTab = newTab;
      }
      if (tab.muted && !newTab.muted) {
        newTab.toggleMuteAudio(tab.muteReason);
      }

      window.gBrowser.removeTab(tab);
    }
  }
}
