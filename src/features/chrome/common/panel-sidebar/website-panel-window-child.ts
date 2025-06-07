/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panels } from "./utils/type";

const PANEL_SIDEBAR_DATA_PREF_NAME = "floorp.panelSidebar.data";

export class WebsitePanelWindowChild {
  private static instance: WebsitePanelWindowChild;
  static getInstance() {
    if (!WebsitePanelWindowChild.instance) {
      WebsitePanelWindowChild.instance = new WebsitePanelWindowChild();
    }
    return WebsitePanelWindowChild.instance;
  }

  currentURL = new URL(window.location.href);

  get panelSidebarData() {
    return JSON.parse(
      Services.prefs.getStringPref(PANEL_SIDEBAR_DATA_PREF_NAME, "{}"),
    ).data as Panels;
  }

  get mainWindow() {
    return document?.getElementById("main-window") as HTMLDivElement;
  }

  get loadURL() {
    return this.webpanelData?.url ?? "";
  }

  get webpanelId() {
    const webpanelId = this.currentURL.searchParams.get("floorpWebPanelId");
    if (!webpanelId) {
      console.error("No webpanelId found");
      return null;
    }
    return webpanelId;
  }

  get userContextId() {
    return this.webpanelData?.userContextId ?? 0;
  }

  get userAgent() {
    return this.webpanelData?.userAgent ? this.webpanelData.userAgent : false;
  }

  get webpanelData() {
    const id = this.webpanelId;
    for (const panel of this.panelSidebarData) {
      if (panel.id === id) {
        return panel;
      }
    }
    return null;
  }

  get isBmsWindow() {
    return Boolean(this.webpanelId);
  }

  constructor() {
    if (!this.webpanelId) {
      return;
    }

    window.floorpWebPanelWindow = true;
    window.SessionStore.promiseInitialized.then(() =>
      this.createWebpanelWindow(),
    );
  }

  setZoomLevel() {
    const zoomLevel = this.webpanelData?.zoomLevel;
    if (zoomLevel) {
      window.ZoomManager.zoom = zoomLevel;
    }
  }

  handleUserContext() {
    let triggeringPrincipal: Principal | null = null;

    const tab = window.gBrowser.selectedTab;
    if (tab.getAttribute("usercontextid") === this.userContextId) {
      return;
    }

    if (tab.linkedPanel) {
      triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
    } else {
      const tabState = JSON.parse(window.SessionStore.getTabState(tab));
      try {
        triggeringPrincipal = window.E10SUtils.deserializePrincipal(
          tabState.triggeringPrincipal_base64,
        );
      } catch (ex) {
        console.error(
          "Failed to deserialize triggeringPrincipal for lazy tab browser",
          ex,
        );
      }
    }

    if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal({
        userContextId: this.userContextId,
      });
    } else if (triggeringPrincipal.isContentPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
        triggeringPrincipal,
        {
          userContextId: this.userContextId,
        },
      );
    }

    const newTab = window.gBrowser.addTab(window.bmsLoadedURI, {
      userContextId: this.userContextId,
      triggeringPrincipal,
    });

    if (window.gBrowser.selectedTab === tab) {
      window.gBrowser.selectedTab = newTab;
    }

    window.gBrowser.removeTab(tab);
  }

  createWebpanelWindow() {
    const { userContextId, userAgent, loadURL, mainWindow } = this;

    mainWindow.setAttribute("BSM-window", "true");
    mainWindow.setAttribute("BMS-usercontextid", userContextId.toString());
    mainWindow.setAttribute("BMS-useragent", userAgent.toString());
    mainWindow.setAttribute("BMS-webpanelid", this.webpanelId ?? "");
    document?.getElementById("navigator-toolbox")?.setAttribute("hidden", "true");
    document?.getElementById("browser")?.setAttribute("data-is-child", "true");
    window.bmsLoadedURI = loadURL;

    // Remove "navigator:browser" from window-main attribute
    mainWindow.setAttribute("windowtype", "navigator:webpanel");

    // Tab modifications
    window.gBrowser.loadURI(Services.io.newURI(loadURL as string), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    // Attribute modifications
    window.gBrowser.selectedTab.setAttribute("BMS-webpanel-tab", "true");

    // Handle issue of opening URL by another application and not loading on the main window
    window.setTimeout(() => {
      const tab = window.gBrowser.addTrustedTab("about:blank");
      window.gBrowser.removeTab(tab);
    }, 0);

    // User context handling
    this.handleUserContext();

    // Window modifications
    mainWindow.setAttribute(
      "chromehidden",
      "toolbar menubar directories extrachrome",
    );

    // Set zoom level
    Services.prefs.addObserver(PANEL_SIDEBAR_DATA_PREF_NAME, () => {
      this.setZoomLevel();
    });

    document?.querySelector(".titlebar-buttonbox-container[skipintoolbarset]")?.remove();
  }
}
