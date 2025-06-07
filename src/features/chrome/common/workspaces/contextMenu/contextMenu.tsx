/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TWorkspaceID } from "../utils/type";
import { WorkspacesService } from "../workspacesService";

export function ContextMenu(props: {
  disableBefore: boolean;
  disableAfter: boolean;
  contextWorkspaceId: TWorkspaceID;
  ctx: WorkspacesService
}) {
  return (
    <>
      <xul:menuitem
        data-l10n-id="reorder-this-workspace-to-up"
        label="Move this Workspace Up"
        disabled={props.disableBefore}
        onCommand={() =>
          //TODO: validate ID
          props.ctx.reorderWorkspaceUp(props.contextWorkspaceId)
        }
      />
      <xul:menuitem
        data-l10n-id="reorder-this-workspace-to-down"
        label="Move this Workspace Down"
        disabled={props.disableAfter}
        onCommand={() =>
          //TODO: validate ID
          props.ctx.reorderWorkspaceDown(props.contextWorkspaceId)
        }
      />
      <xul:menuseparator class="workspaces-context-menu-separator" />
      <xul:menuitem
        data-l10n-id="delete-this-workspace"
        label="Delete Workspace"
        onCommand={() =>
          //TODO: validate ID
          props.ctx.deleteWorkspace(props.contextWorkspaceId)
        }
      />
      <xul:menuitem
        data-l10n-id="manage-this-workspaces"
        label="Manage Workspace"
        onCommand={() =>
          //TODO: validate ID
          props.ctx.manageWorkspaceFromDialog(props.contextWorkspaceId)
        }
      />
    </>
  );
}
