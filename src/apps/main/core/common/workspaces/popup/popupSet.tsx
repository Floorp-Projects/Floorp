/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "@core/utils/context-menu";
import { WorkspacesService } from "../workspacesService.js";
import { ContextMenu } from "./contextMenu.js";
import { render } from "@nora/solid-xul";
import { workspacesDataStore } from "../data/data.js";

export class WorkspacesPopupContxtMenu {
  ctx:WorkspacesService
  constructor(ctx:WorkspacesService) {
    this.ctx=ctx;
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.PopupSet(),import.meta.hot);
  }
  /**
   * Create context menu items for workspaces.
   * @param event The event.
   * @returns The context menu items.
   */
  private createworkspacesContextMenuItems(event: Event) {
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
    const defaultWorkspaceId = workspacesDataStore.defaultID

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
          ctx:this.ctx
        }),
      parentElem,
    );
  }

  private PopupSet() {
    return (
      <xul:popupset>
        <xul:menupopup
          id="workspaces-toolbar-item-context-menu"
          onPopupShowing={(event) =>
            this.createworkspacesContextMenuItems(event)
          }
        />
      </xul:popupset>
    );
  }

  
}
