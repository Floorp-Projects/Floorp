/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "@core/utils/context-menu";
import { WorkspacesServices } from "./workspaces.js";
import { ContextMenu } from "./contextMenu.js";
import { render } from "@nora/solid-xul";

export class WorkspacesPopupContxtMenu {
  private static instance: WorkspacesPopupContxtMenu;
  public static getInstance() {
    if (!WorkspacesPopupContxtMenu.instance) {
      WorkspacesPopupContxtMenu.instance = new WorkspacesPopupContxtMenu();
    }
    return WorkspacesPopupContxtMenu.instance;
  }

  /**
   * Create context menu items for workspaces.
   * @param event The event.
   * @returns The context menu items.
   */
  private createworkspacesContextMenuItems(event: Event) {
    const gWorkspacesServices = WorkspacesServices.getInstance();
    //delete already exsist items
    const menuElem = document?.getElementById(
      "workspaces-toolbar-item-context-menu",
    );
    while (menuElem?.firstChild) {
      const firstChild = menuElem.firstChild as XULElement;
      firstChild.remove();
    }

    const eventTargetElement = event.explicitOriginalTarget as XULElement;
    const contextWorkspaceId = eventTargetElement.id.replace("workspace-", "");
    const defaultWorkspaceId = gWorkspacesServices.getDefaultWorkspaceId();

    const beforeSiblingElem =
      eventTargetElement.previousElementSibling?.getAttribute(
        "data-workspaceId",
      ) || null;
    const afterSiblingElem =
      eventTargetElement.nextElementSibling?.getAttribute("data-workspaceId") ||
      null;

    const isDefaultWorkspace = contextWorkspaceId === defaultWorkspaceId;
    const isBeforeSiblingDefaultWorkspace =
      beforeSiblingElem === defaultWorkspaceId;
    const isAfterSiblingExist = afterSiblingElem != null;
    const needDisableBefore =
      isDefaultWorkspace || isBeforeSiblingDefaultWorkspace;
    const needDisableAfter = isDefaultWorkspace || !isAfterSiblingExist;

    //create context menu
    const parentElem = document?.getElementById(
      "workspaces-toolbar-item-context-menu",
    );
    render(
      () =>
        ContextMenu({
          disableBefore: needDisableBefore,
          disableAfter: needDisableAfter,
          contextWorkspaceId,
        }),
      parentElem,
      {
        hotCtx: import.meta.hot,
      },
    );
  }

  private PopupSet() {
    return (
      <xul:popupset>
        <xul:menupopup
          id="workspaces-toolbar-item-context-menu"
          onpopupshowing={(event) =>
            this.createworkspacesContextMenuItems(event)
          }
        />
      </xul:popupset>
    );
  }

  constructor() {
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.PopupSet(),import.meta.hot);
  }
}
