/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { panelSidebarConfig } from "./data/data";
import { CPanelSidebar } from "./components/panel-sidebar";
import { WebsitePanel } from "./website-panel-window-parent";

export namespace PanelNavigator {
  let _gPanelSidebar: CPanelSidebar | null = null;

  export function setGPanelSidebar(sidebar: CPanelSidebar | null) {
    _gPanelSidebar = sidebar;
  }

  export function getGPanelSidebar(): CPanelSidebar | null {
    return _gPanelSidebar;
  }

  const gWebsitePanel = WebsitePanel.getInstance();
  const addonEnabled = panelSidebarConfig().webExtensionRunningEnabled;

  function goHome(browser: XULElement, sideBarId: string) {
    browser.src = "";
    browser.src = getGPanelSidebar()!.getPanelData(sideBarId)?.url ?? "";
  }

  /* Navigation */
  export function back(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.goBackPanel(sideBarId)
      : getGPanelSidebar()!.getBrowserElement(sideBarId)?.goBack();
  }

  export function forward(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.goForwardPanel(sideBarId)
      : getGPanelSidebar()!.getBrowserElement(sideBarId)?.goForward();
  }

  export function reload(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.reloadPanel(sideBarId)
      : getGPanelSidebar()!.getBrowserElement(sideBarId)?.reload();
  }

  export function goIndexPage(sideBarId: string) {
    addonEnabled ? gWebsitePanel.goIndexPagePanel(sideBarId) : goHome(
      getGPanelSidebar()!.getBrowserElement(sideBarId) as XULElement,
      sideBarId,
    );
  }

  /* Mute/Unmute */
  export function toggleMute(sideBarId: string) {
    addonEnabled
      ? gWebsitePanel.toggleMutePanel(sideBarId)
      : getGPanelSidebar()!.getBrowserElement(sideBarId)?.toggleMute();
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
