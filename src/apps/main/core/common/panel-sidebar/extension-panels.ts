/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { MapSidebars, Sidebar } from "./utils/type";

const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

export function getFirefoxSidebarPanels(): Sidebar[] {
  return Array.from(window.SidebarController.sidebars as MapSidebars)
    .filter((sidebar) => {
      return sidebar[1].extensionId;
    })
    .map((sidebar) => {
      return sidebar[1];
    });
}

export function isExtensionExist(extensionId: string): boolean {
  return getFirefoxSidebarPanels().some(
    (panel) => panel.extensionId === extensionId,
  );
}

export function getSidebarIconFromSidebarController(extensionId: string) {
  return getFirefoxSidebarPanels().find(
    (panel) => panel.extensionId === extensionId,
  )?.iconUrl;
}

export function getExtensionSidebarAction(extensionId: string) {
  const policy = WebExtensionPolicy.getByID(extensionId);
  return policy?.extension.manifest.sidebar_action;
}
