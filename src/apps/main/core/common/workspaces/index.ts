/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WorkspacesServices } from "./workspaces.js";
import { WorkspacesToolbarButton } from "./toolbar-element.js";
import { WorkspacesTabContextMenu } from "./tabContextMenu.js";
import { WorkspacesPopupContxtMenu } from "./popupSet.js";
import { WorkspaceManageModal } from "./workspace-modal.js";
import { WorkspaceIcons } from "./utils/workspace-icons.js";
import { enabled } from "./config.js";
import { noraComponent, NoraComponentBase } from "@core/utils/base.js";

//TODO: Currently refactoring is required esp. for HMR

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static ctx:WorkspacesServices | null = null;
  
  init(): void {
    const iconCtx = new WorkspaceIcons();
    iconCtx.initializeIcons()
    // If workspaces is not enabled, do not proceed.
    if (!enabled()) {
      return;
    }

    const ctx = new WorkspacesServices(iconCtx);
    new WorkspacesTabContextMenu(ctx);
    new WorkspacesToolbarButton(ctx);
    new WorkspacesPopupContxtMenu(ctx);
    new WorkspaceManageModal(ctx,iconCtx);
    Workspaces.ctx=ctx;
  }
}