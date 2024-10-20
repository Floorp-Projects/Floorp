/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

/* zod schemas */
export const zWorkspace = z.object({
  id: z.string(),
  name: z.string(),
  icon: z.string().nullish(),
  userContextId: z.number(),
  isSelected: z.boolean().nullish(),
  isDefault: z.boolean().nullish(),
});

export const zWorkspacesServices = z.array(zWorkspace);

export const zWorkspacesServicesStoreData = z.array(zWorkspace);
export const zWorkspaceBackupTab = z.object({
  title: z.string(),
  url: z.string(),
  favicon: z.string(),
  pinned: z.boolean(),
  selected: z.boolean(),
});

export const zWorkspaceBackup = z.object({
  workspace: z.array(
    zWorkspace.merge(
      z.object({
        tabs: z.array(zWorkspaceBackupTab),
      }),
    ),
  ),
});

export const zWorkspacesServicesBackup = z.object({
  workspaces: z.array(zWorkspaceBackup),
  currentWorkspaceId: z.string(),
  timestamp: z.number(),
});

export const zWorkspacesServicesConfigs = z.object({
  manageOnBms: z.boolean(),
  showWorkspaceNameOnToolbar: z.boolean(),
  closePopupAfterClick: z.boolean(),
});

/* Export as types */
export type Workspace = z.infer<typeof zWorkspace>;
export type Workspaces = z.infer<typeof zWorkspacesServices>;
export type WorkspacesStoreData = z.infer<typeof zWorkspacesServicesStoreData>;
export type WorkspaceBackupTab = z.infer<typeof zWorkspaceBackupTab>;
export type WorkspaceBackup = z.infer<typeof zWorkspaceBackup>;
export type WorkspacesBackup = z.infer<typeof zWorkspacesServicesBackup>;
export type WorkspacesServicesConfigsType = z.infer<
  typeof zWorkspacesServicesConfigs
>;

/* XUL Elements */
export type PanelMultiViewParentElement = XULElement & {
  hidePopup: () => void;
};
