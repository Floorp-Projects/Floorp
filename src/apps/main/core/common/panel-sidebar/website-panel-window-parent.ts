/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setPanelSidebarData } from "./data/data.ts";

type XULBrowserElement = XULElement & {
  browsingContext: {
    associatedWindow: Window;
  };
};

export class WebsitePanel {
  private static instance: WebsitePanel;

  static getInstance() {
    if (!WebsitePanel.instance) {
      WebsitePanel.instance = new WebsitePanel();
    }
    return WebsitePanel.instance;
  }

  private getWindowByWebpanelId(id: string, parentWindow: Window) {
    const webpanelBrowserId = `sidebar-panel-${id}`;
    const webpanelBrowser = parentWindow?.document?.getElementById(
      webpanelBrowserId,
    ) as XULBrowserElement;

    if (!webpanelBrowser) {
      throw new Error("Target panel window not found");
    }

    return webpanelBrowser.browsingContext.associatedWindow;
  }

  public toggleMutePanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      const tab = targetPanelWindow.gBrowser.selectedTab;

      const audioMuted = tab.linkedBrowser.audioMuted;
      tab.linkedBrowser.audioMuted = !audioMuted;
    } catch (e) {
      console.error("Failed to toggle mute for webpanel", e);
    }
  }

  public reloadPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      const tab = targetPanelWindow.gBrowser.selectedTab;

      tab.linkedBrowser.reload();
    } catch (e) {
      console.error("Failed to reload webpanel", e);
    }
  }

  public goForwardPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.gBrowser.selectedTab.linkedBrowser.goForward();
    } catch (e) {
      console.error("Failed to go forward in webpanel", e);
    }
  }

  public goBackPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.gBrowser.selectedTab.linkedBrowser.goBack();
    } catch (e) {
      console.error("Failed to go back in webpanel", e);
    }
  }

  public goIndexPagePanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      const uri = targetPanelWindow.bmsLoadedURI;

      targetPanelWindow.gBrowser.loadURI(Services.io.newURI(uri), {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
    } catch (e) {
      console.error("Failed to go to index page in webpanel", e);
    }
  }

  private saveZoomLevel(webpanelId: string, zoomLevel: number) {
    setPanelSidebarData((prev) => {
      Object.values(prev).forEach((panel) => {
        if (panel.id === webpanelId) {
          panel.zoomLevel = zoomLevel;
        }
      });
      return prev;
    });
  }

  public zoomInPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);

      targetPanelWindow.ZoomManager.enlarge();
      const newZoomLevel = targetPanelWindow.ZoomManager.zoom;
      this.saveZoomLevel(webpanelId, newZoomLevel);
    } catch (e) {
      console.error("Failed to zoom in webpanel", e);
    }
  }

  public zoomOutPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);

      targetPanelWindow.ZoomManager.reduce();
      const newZoomLevel = targetPanelWindow.ZoomManager.zoom;
      this.saveZoomLevel(webpanelId, newZoomLevel);
    } catch (e) {
      console.error("Failed to zoom out webpanel", e);
    }
  }

  public resetZoomLevelPanel(webpanelId: string) {
    try {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);

      targetPanelWindow.ZoomManager.zoom = 1;
      this.saveZoomLevel(webpanelId, 1);
    } catch (e) {
      console.error("Failed to reset zoom in webpanel", e);
    }
  }
}
