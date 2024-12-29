/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { panelSidebarConfig } from "./data/data";
import { CPanelSidebar } from "./components/panel-sidebar";
import { WebsitePanel } from "./website-panel-window-parent";

export namespace PanelNavigator {
  export let gPanelSidebar: CPanelSidebar | null = null;
  const gWebsitePanel = WebsitePanel.getInstance();
  const addonEnabled = panelSidebarConfig().webExtensionRunningEnabled;

  function goHome(browser: XULElement, sideBarId: string) {
    browser.src = "";
    browser.src = gPanelSidebar!.getPanelData(sideBarId)?.url ?? "";
  }

  /* Navigation */
  export function back(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.goBackPanel(sideBarId)
      : gPanelSidebar!.getBrowserElement(sideBarId)?.goBack();
  }

  export function forward(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.goForwardPanel(sideBarId)
      : gPanelSidebar!.getBrowserElement(sideBarId)?.goForward();
  }

  export function reload(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.reloadPanel(sideBarId)
      : gPanelSidebar!.getBrowserElement(sideBarId)?.reload();
  }

  export function goIndexPage(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.goIndexPagePanel(sideBarId)
      : goHome(
          gPanelSidebar!.getBrowserElement(sideBarId) as XULElement,
          sideBarId,
        );
  }

  /* Mute/Unmute */
  export function toggleMute(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.toggleMutePanel(sideBarId)
      : gPanelSidebar!.getBrowserElement(sideBarId)?.toggleMute();
  }

  /* Zoom */
  export function zoomIn(sideBarId: string) {
    gWebsitePanel.zoomInPanel(sideBarId);
  }

  export function zoomOut(sideBarId: string) {
    gWebsitePanel.zoomOutPanel(sideBarId);
  }

  export function zoomReset(sideBarId: string) {
    gWebsitePanel.resetZoomLevelPanel(sideBarId);
  }
}
