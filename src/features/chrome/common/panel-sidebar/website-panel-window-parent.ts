/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setPanelSidebarData } from "./data/data.js";

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

  /* Mute/Unmute */
  public toggleMutePanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const tab = targetPanelWindow.gBrowser.selectedTab;
    const audioMuted = tab.linkedBrowser.audioMuted;

    if (audioMuted) {
      tab.linkedBrowser.unmute();
    } else {
      tab.linkedBrowser.mute();
    }
  }

  /* Reload */
  public reloadPanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const tab = targetPanelWindow.gBrowser.selectedTab;

    tab.linkedBrowser.reload();
  }

  /* Forward */
  public goForwardPanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const tab = targetPanelWindow.gBrowser.selectedTab;

    tab.linkedBrowser.goForward();
  }

  /* Back */
  public goBackPanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const tab = targetPanelWindow.gBrowser.selectedTab;

    tab.linkedBrowser.goBack();
  }

  /* Go Index Page */
  public goIndexPagePanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const uri = targetPanelWindow.bmsLoadedURI;

    targetPanelWindow.gBrowser.loadURI(Services.io.newURI(uri), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  }

  /* Zoom Level */
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
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const zoomLevel = targetPanelWindow.ZoomManager.zoom;

    targetPanelWindow.ZoomManager.enlarge();
    this.saveZoomLevel(webpanelId, zoomLevel);
  }

  public zoomOutPanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const zoomLevel = targetPanelWindow.ZoomManager.zoom;

    targetPanelWindow.ZoomManager.reduce();
    this.saveZoomLevel(webpanelId, zoomLevel);
  }

  public resetZoomLevelPanel(webpanelId: string) {
    const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
    const zoomLevel = targetPanelWindow.ZoomManager.zoom;

    targetPanelWindow.ZoomManager.zoom = 1;
    this.saveZoomLevel(webpanelId, zoomLevel);
  }
}
