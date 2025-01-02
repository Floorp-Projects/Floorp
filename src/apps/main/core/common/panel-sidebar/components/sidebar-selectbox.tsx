/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createResource, For, lazy, Suspense } from "solid-js";
import { panelSidebarData } from "../data/data";
import { PanelSidebarButton } from "./sidebar-panel-button";
import { setPanelSidebarAddModalState } from "./panel-sidebar-modal";
import { CPanelSidebar } from "./panel-sidebar";
import { createRootHMR } from "@nora/solid-xul";
import { getFaviconURLForPanel } from "../utils/favicon-getter";

export function SidebarSelectbox(props: {ctx:CPanelSidebar}) {
  return (
    <xul:vbox
      id="panel-sidebar-select-box"
      class="webpanel-box chromeclass-extrachrome"
    >
      <For each={panelSidebarData()}>
        {(panel) =>

          <PanelSidebarButton panel={panel} ctx={props.ctx} />
          }

      </For>
      <xul:toolbarbutton
        id="panel-sidebar-add"
        class="panel-sidebar-panel"
        onCommand={() => {
          setPanelSidebarAddModalState(true);
        }}
      />
      <xul:spacer flex="1" />
      <xul:vbox id="panel-sidebar-bottomButtonBox">
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          data-l10n-id="sidebar2-hide-sidebar"
          onCommand={() =>
            Services.prefs.setBoolPref("floorp.browser.sidebar.enable", false)
          }
          id="panel-sidebar-hide-icon"
        />
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          data-l10n-id="sidebar-addons-button"
          onCommand={() =>
            window.BrowserAddonUI.openAddonsMgr("addons://list/extension")
          }
          id="panel-sidebar-addons-icon"
        />
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          data-l10n-id="sidebar-passwords-button"
          onCommand={() =>
            window.LoginHelper.openPasswordManager(window, {
              entryPoint: "mainmenu",
            })
          }
          id="panel-sidebar-passwords-icon"
        />
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          data-l10n-id="sidebar-preferences-button"
          onCommand={() => window.openPreferences()}
          id="panel-sidebar-preferences-icon"
        />
      </xul:vbox>
    </xul:vbox>
  );
}
