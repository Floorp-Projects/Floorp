/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TWorkspaceID } from "../utils/type.js";
import { WorkspacesService } from "../workspacesService";

export function PopupToolbarElement(props: {
  workspaceId: TWorkspaceID;
  isSelected: boolean;
  label: string;
  bmsMode: boolean;
  ctx:WorkspacesService,
}) {
 
  const workspace = props.ctx.getRawWorkspace(props.workspaceId)
  return (
    <xul:toolbarbutton
      id={`workspace-${props.workspaceId}`}
      label={props.label}
      context="workspaces-toolbar-item-context-menu"
      class="toolbarbutton-1 chromeclass-toolbar-additional workspaceButton"
      style={{
        "list-style-image": `url(${ props.ctx.iconCtx.getWorkspaceIconUrl(workspace.icon)})`,
      }}
      data-selected={props.isSelected}
      data-workspaceId={props.workspaceId}
      onCommand={() => {
        props.ctx.changeWorkspace(props.workspaceId);
      }}
    />
  );
}
