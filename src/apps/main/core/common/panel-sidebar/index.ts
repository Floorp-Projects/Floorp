/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PanelSidebar } from "./panel-sidebar";
import { PanelSidebarElem } from "./sidebar";
import { SidebarContextMenuElem } from "./sidebar-contextMenu";
import { migratePanelSidebarData } from "./migration";
import { WebsitePanelWindowChild } from "./website-panel-window-child";
import { PanelSidebarAddModal } from "./panel-sidebar-modal";
import { PanelSidebarFloating } from "./floating";

export async function init() {
  migratePanelSidebarData(() => {
    WebsitePanelWindowChild.getInstance();
    PanelSidebarElem.getInstance();
    SidebarContextMenuElem.getInstance();
    PanelSidebarAddModal.getInstance();
    PanelSidebarFloating.getInstance();
    PanelSidebar.getInstance();
  });
}
