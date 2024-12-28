/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PanelSidebarStaticNames } from "./utils/panel-sidebar-static-names";
import type { PanelSidebarConfig } from "./utils/type";

export async function migratePanelSidebarData() {
  const oldData = Services.prefs.getCharPref(
    "floorp.browser.sidebar2.data",
    undefined,
  );

  if (oldData) {
    const newSidebar = convertSidebar(JSON.parse(oldData) as OldSidebar);
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      JSON.stringify(newSidebar),
    );

    Services.prefs.clearUserPref("floorp.browser.sidebar2.data");

    // Create a new config
    const globalWidth = Services.prefs.getIntPref(
      "floorp.browser.sidebar2.global.webpanel.width",
      400,
    );

    const autoUnload = Services.prefs.getBoolPref(
      "floorp.browser.sidebar2.hide.to.unload.panel.enabled",
      false,
    );

    const position_start = Services.prefs.getBoolPref(
      "floorp.browser.sidebar.right",
      true,
    );

    const displayed = Services.prefs.getBoolPref(
      "floorp.browser.sidebar.is.displayed",
      true,
    );

    const config: PanelSidebarConfig = {
      globalWidth,
      autoUnload,
      position_start,
      displayed,
      webExtensionRunningEnabled: false,
    };

    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarConfigPrefName,
      JSON.stringify({ config }),
    );
  }
}

type OldSidebarData = {
  url: string;
  width?: number;
  usercontext?: number;
  zoomLevel?: number;
};

type OldSidebar = {
  data: { [key: string]: OldSidebarData };
  index: string[];
};

type NewSidebarItem = {
  id: string;
  type: "extension" | "static" | "web";
  width: number;
  url: string;
  userContextId: number | null;
  zoomLevel: number | null;
};

type NewSidebar = {
  data: NewSidebarItem[];
};

function convertSidebar(oldSidebar: OldSidebar): NewSidebar {
  const newSidebar: NewSidebar = { data: [] };

  oldSidebar.index.forEach((key) => {
    const item = oldSidebar.data[key];
    const url = item.url;
    let type: "extension" | "static" | "web";
    const id = key;
    let width = item.width || 0;
    const userContextId = item.usercontext || null;
    const zoomLevel = item.zoomLevel || null;

    if (url.startsWith("extension")) {
      type = "extension";
      width = width || 450; // 任意のデフォルト値
    } else if (url.startsWith("floorp//")) {
      type = "static";
    } else {
      type = "web";
    }

    newSidebar.data.push({
      id,
      type,
      width,
      url,
      userContextId,
      zoomLevel,
    });
  });

  return newSidebar;
}
