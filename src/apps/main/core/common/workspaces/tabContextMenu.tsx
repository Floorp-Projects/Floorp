/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { WorkspacesService } from "./workspacesService";
import { For } from "solid-js";
import { workspacesDataStore } from "./data/data.js";
import { TWorkspaceID } from "./utils/type.js";

export class WorkspacesTabContextMenu {
  ctx:WorkspacesService;
  constructor(ctx:WorkspacesService) {
    this.ctx=ctx;
    const parentElem = document?.getElementById("tabContextMenu");
    render(() => this.contextMenu(), parentElem, {
      marker: document?.getElementById("context_moveTabOptions") as XULElement,
    });
  }

  // static is Against "this.menuItem is not a function" error.
  private menuItem(order: string[]) {
    const gWorkspaceIcons = this.ctx.iconCtx;
    return (
      <For each={order}>
        {(id) => {
          if (this.ctx.isWorkspaceID(id)) {
            const workspace = this.ctx.getRawWorkspace(id);
            return <xul:menuitem
              id="context_MoveTabToOtherWorkspace"
              label={workspace.name}
              class="menuitem-iconic"
              style={`list-style-image: url(${gWorkspaceIcons.getWorkspaceIconUrl(workspace.icon)})`}
              onCommand={() =>
                this.ctx.tabManagerCtx.moveTabsToWorkspaceFromTabContextMenu(id)
              }
            />
          } else {
            console.error("Not valid ID for Workspaces (maybe order is not updated) : "+id);
          }
        }}
      </For>
    );
  }

  public contextMenu() {
    return (
      <xul:menu
        id="context_MoveTabToOtherWorkspace"
        data-l10n-id="move-tab-another-workspace"
        accesskey="D"
      >
        <xul:menupopup
          id="WorkspacesTabContextMenu"
          onPopupShowing={this.createTabworkspacesContextMenuItems}
        />
      </xul:menu>
    );
  }

  public createTabworkspacesContextMenuItems(this: WorkspacesTabContextMenu) {
    const menuElem = document?.getElementById("WorkspacesTabContextMenu");
    while (menuElem?.firstChild) {
      const child = menuElem.firstChild as XULElement;
      child.remove();
    }

    //create context menu
    const tabWorkspaceId = this.ctx.tabManagerCtx.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    const excludeHasTabWorkspaceIdWorkspaces = workspacesDataStore.order.filter((w)=>w !== tabWorkspaceId)

    const parentElem = document?.getElementById("WorkspacesTabContextMenu");
    render(
      () =>
        this.menuItem(excludeHasTabWorkspaceIdWorkspaces),
      parentElem,
    );
  }
}
