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

export function init() {
  const gWorkspaceIcons = WorkspaceIcons.getInstance();
  gWorkspaceIcons.initializeIcons().then(() => {
    // If workspaces is not enabled, do not proceed.
    if (!enabled()) {
      return;
    }

    WorkspacesServices.getInstance();
    WorkspacesTabContextMenu.getInstance();
    WorkspacesToolbarButton.getInstance();
    WorkspacesPopupContxtMenu.getInstance();
    WorkspaceManageModal.getInstance();
  });
}
