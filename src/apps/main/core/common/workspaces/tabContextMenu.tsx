/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import type { WorkspacesService } from "./workspacesService.ts";
import { For } from "solid-js";
import { workspacesDataStore } from "./data/data.ts";
import i18next from "i18next";
import { addI18nObserver } from "../../../i18n/config.ts";

const translationKeys = {
  moveTabToAnotherWorkspace: "workspaces.menu.moveTabToAnotherWorkspace",
  invalidWorkspaceID: "workspaces.error.invalidWorkspaceID"
};

const getTranslatedText = (key: string): string => {
  return i18next.t(key);
};

export class WorkspacesTabContextMenu {
  ctx: WorkspacesService;
  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    const parentElem = document?.getElementById("tabContextMenu");
    render(() => this.contextMenu(), parentElem, {
      marker: document?.getElementById("context_moveTabOptions") as XULElement,
    });

    addI18nObserver(() => {
      this.updateContextMenu();
    });
  }

  private updateContextMenu() {
    const menuElem = document?.getElementById("context_MoveTabToOtherWorkspace");
    if (menuElem) {
      menuElem.setAttribute("label", getTranslatedText(translationKeys.moveTabToAnotherWorkspace));
    }
  }

  private menuItem(order: string[]) {
    return (
      <For each={order}>
        {(id) => {
          if (this.ctx.isWorkspaceID(id)) {
            const workspace = () => this.ctx.getRawWorkspace(id);
            const icon = () =>
              this.ctx.iconCtx.getWorkspaceIconUrl(workspace().icon);
            return (
              <xul:menuitem
                id="context_MoveTabToOtherWorkspace"
                label={workspace().name}
                class="menuitem-iconic"
                style={`list-style-image: url(${icon()})`}
                onCommand={() =>
                  this.ctx.tabManagerCtx.moveTabsToWorkspaceFromTabContextMenu(
                    id,
                  )
                }
              />
            );
          } else {
            console.error(
              getTranslatedText(translationKeys.invalidWorkspaceID) + ": " + id
            );
          }
        }}
      </For>
    );
  }

  public contextMenu() {
    return (
      <xul:menu
        id="context_MoveTabToOtherWorkspace"
        label={getTranslatedText(translationKeys.moveTabToAnotherWorkspace)}
        accesskey="D"
      >
        <xul:menupopup
          id="WorkspacesTabContextMenu"
          onPopupShowing={() => this.createTabworkspacesContextMenuItems()}
        />
      </xul:menu>
    );
  }

  public createTabworkspacesContextMenuItems() {
    const menuElem = document?.getElementById("WorkspacesTabContextMenu");
    while (menuElem?.firstChild) {
      const child = menuElem.firstChild as XULElement;
      child.remove();
    }

    //create context menu
    const tabWorkspaceId = this.ctx.tabManagerCtx.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    const excludeHasTabWorkspaceIdWorkspaces = workspacesDataStore.order.filter(
      (w) => w !== tabWorkspaceId,
    );

    const parentElem = document?.getElementById("WorkspacesTabContextMenu");
    render(() => this.menuItem(excludeHasTabWorkspaceIdWorkspaces), parentElem);
  }
}
