/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { WorkspaceIcons } from "./utils/workspace-icons.js";
import { WorkspacesServices } from "./workspaces.js";
import { For } from "solid-js";
import { workspacesData } from "./data.js";
import type { Workspaces } from "./utils/type.js";

export class WorkspacesTabContextMenu {
  private static instance: WorkspacesTabContextMenu;
  public static getInstance() {
    if (!WorkspacesTabContextMenu.instance) {
      WorkspacesTabContextMenu.instance = new WorkspacesTabContextMenu();
    }
    return WorkspacesTabContextMenu.instance;
  }

  // static is Against "this.menuItem is not a function" error.
  private static menuItem(workspaces: Workspaces) {
    const gWorkspaces = WorkspacesServices.getInstance();
    const gWorkspaceIcons = WorkspaceIcons.getInstance();
    return (
      <For each={workspaces}>
        {(workspace) => (
          <xul:menuitem
            id="context_MoveTabToOtherWorkspace"
            label={workspace.name}
            class="menuitem-iconic"
            style={`list-style-image: url(${gWorkspaceIcons.getWorkspaceIconUrl(workspace.icon)})`}
            onCommand={() =>
              gWorkspaces.moveTabsToWorkspaceFromTabContextMenu(workspace.id)
            }
          />
        )}
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
          onpopupshowing={this.createTabworkspacesContextMenuItems}
        />
      </xul:menu>
    );
  }

  public createTabworkspacesContextMenuItems(this: WorkspacesTabContextMenu) {
    const gWorkspacesServices = WorkspacesServices.getInstance();
    const menuElem = document?.getElementById("WorkspacesTabContextMenu");
    while (menuElem?.firstChild) {
      const child = menuElem.firstChild as XULElement;
      child.remove();
    }

    //create context menu
    const tabWorkspaceId = gWorkspacesServices.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    const excludeHasTabWorkspaceIdWorkspaces = workspacesData().filter(
      (workspace) => workspace.id !== tabWorkspaceId,
    ) as Workspaces;

    const parentElem = document?.getElementById("WorkspacesTabContextMenu");
    render(
      () =>
        WorkspacesTabContextMenu.menuItem(excludeHasTabWorkspaceIdWorkspaces),
      parentElem,
      {
        hotCtx: import.meta.hot,
      },
    );
  }

  constructor() {
    const parentElem = document?.getElementById("tabContextMenu");
    render(() => this.contextMenu(), parentElem, {
      marker: document?.getElementById("context_moveTabOptions") as XULElement,
      hotCtx: import.meta.hot,
    });
  }
}
