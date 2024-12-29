/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Show } from "solid-js";
import {
  selectedPanelId,
  setSelectedPanelId,
  setIsFloating,
  isFloating,
} from "./data";
import { PanelNavigator } from "./panel-navigator";
import { CPanelSidebar } from "./panel-sidebar";

export function SidebarHeader(props:{ctx:CPanelSidebar}) {
  const gPanelSidebar = props.ctx;
  PanelNavigator.gPanelSidebar = gPanelSidebar;
  return (
    <xul:box id="panel-sidebar-header" align="center">
      <Show
        when={
          gPanelSidebar.getPanelData(selectedPanelId() ?? "")?.type === "web"
        }
      >
        <xul:toolbarbutton
          id="panel-sidebar-back"
          class="panel-sidebar-actions toolbarbutton-1 chromeclass-toolbar-additional"
          data-l10n-id="sidebar-back-button"
        />
        <xul:toolbarbutton
          id="panel-sidebar-forward"
          onCommand={() => PanelNavigator.forward(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
          data-l10n-id="sidebar-forward-button"
        />
        <xul:toolbarbutton
          id="panel-sidebar-reload"
          onCommand={() => PanelNavigator.reload(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
          data-l10n-id="sidebar-reload-button"
        />
        <xul:toolbarbutton
          id="panel-sidebar-go-index"
          onCommand={() => PanelNavigator.goIndexPage(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
          data-l10n-id="sidebar-go-index-button"
        />
      </Show>
      <xul:spacer flex="1" />
      <Show
        when={
          gPanelSidebar.getPanelData(selectedPanelId() ?? "")?.type === "web"
        }
      >
        <xul:toolbarbutton
          id="panel-sidebar-float"
          onCommand={() => setIsFloating(!isFloating())}
          class="panel-sidebar-actions"
          data-l10n-id="sidebar-float-button"
        />
        <xul:toolbarbutton
          id="panel-sidebar-open-in-main-window"
          onCommand={() =>
            gPanelSidebar.openInMainWindow(selectedPanelId() ?? "")
          }
          class="panel-sidebar-actions"
        />
      </Show>
      <xul:toolbarbutton
        id="panel-sidebar-close"
        onCommand={() => setSelectedPanelId(null)}
        class="panel-sidebar-actions"
      />
    </xul:box>
  );
}
