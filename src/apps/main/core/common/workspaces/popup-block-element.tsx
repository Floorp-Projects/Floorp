/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WorkspaceIcons } from "./utils/workspace-icons.js";
import { WorkspacesServices } from "./workspaces.js";

export function PopupToolbarElement(props: {
  workspaceId: string;
  isSelected: boolean;
  label: string;
  bmsMode: boolean;
  ctx:WorkspacesServices,
}) {
  const gWorkspacesServices = props.ctx;
  const gWorkspaceIcons = props.ctx.iconCtx;
  const { workspaceId, isSelected, bmsMode } = props;
  const workspace = gWorkspacesServices.getWorkspaceById(workspaceId);
  return (
    <xul:toolbarbutton
      id={`workspace-${workspaceId}`}
      label={props.label}
      context="workspaces-toolbar-item-context-menu"
      class="toolbarbutton-1 chromeclass-toolbar-additional workspaceButton"
      style={{
        "list-style-image": `url(${gWorkspaceIcons.getWorkspaceIconUrl(workspace.icon)})`,
      }}
      data-selected={isSelected}
      data-workspaceId={workspaceId}
      onCommand={() => {
        gWorkspacesServices.changeWorkspace(workspaceId);
      }}
    />
  );
}
