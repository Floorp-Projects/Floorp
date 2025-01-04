/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import { WorkspacesService } from "../workspacesService";
import { PopupToolbarElement } from "./popup-block-element";
import { configStore } from "../data/config";
import { workspacesDataStore } from "../data/data.js";

export function PopupElement(props:{ctx:WorkspacesService}) {
  return (
    <xul:panelview
      id="workspacesToolbarButtonPanel"
      type="arrow"
      position={"bottomleft topleft"}
    >
      <xul:vbox id="workspacesToolbarButtonPanelBox">
        <xul:arrowscrollbox id="workspacesPopupBox" flex="1">
          <xul:vbox
            id="workspacesPopupContent"
            align="center"
            flex="1"
            orient="vertical"
            clicktoscroll={true}
            class="statusbar-padding"
          >
            <For each={workspacesDataStore.order}>
              {(id) => {
                  const workspace = workspacesDataStore.data.get(id)!;
                  return <PopupToolbarElement
                  workspaceId={id}
                  label={workspace.name}
                  isSelected={id === workspacesDataStore.selectedID}
                  bmsMode={configStore.manageOnBms}
                  ctx={props.ctx}
                />
              }
              }
            </For>
          </xul:vbox>
        </xul:arrowscrollbox>
        <xul:toolbarseparator id="workspacesPopupSeparator" />
        <xul:hbox id="workspacesPopupFooter" align="center" pack="center">
          <xul:toolbarbutton
            id="workspacesCreateNewWorkspaceButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-create-new-workspace-button"
            label="Create New Workspace..."
            context="tab-stacks-toolbar-item-context-menu"
            onCommand={() => props.ctx.createNoNameWorkspace()}
          />
          <xul:toolbarbutton
            id="workspacesManageworkspacesServicesButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-manage-workspaces-button"
            context="tab-stacks-toolbar-item-context-menu"
            onCommand={() => props.ctx.manageWorkspaceFromDialog()}
          />
        </xul:hbox>
      </xul:vbox>
    </xul:panelview>
  );
}
