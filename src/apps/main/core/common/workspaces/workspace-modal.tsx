/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createResource, createSignal, For, Setter } from "solid-js";
import { createRootHMR, render } from "@nora/solid-xul";
import { WorkspaceIcons } from "./utils/workspace-icons.js";
import type { TWorkspace, TWorkspaceID } from "./utils/type.js";
import { WorkspacesService } from "./workspacesService";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data.js";
import ModalParent from "../modal-parent/index.ts";
import { TForm } from "@core/common/modal-parent/utils/type.ts";

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  name: string;
  userContextId: number;
  l10nId?: string;
};

type ModalState = {
  show: boolean;
  targetWorkspaceID: TWorkspaceID | null;
};

export class WorkspaceManageModal {
  ctx: WorkspacesService;
  iconCtx: WorkspaceIcons;
  private modalParent: ModalParent;

  constructor(ctx: WorkspacesService, iconCtx: WorkspaceIcons) {
    this.ctx = ctx;
    this.iconCtx = iconCtx;
    this.modalParent = ModalParent.getInstance();
    this.modalParent.init();
  }

  private get containers(): Container[] {
    return ContextualIdentityService.getPublicIdentities();
  }

  private getContainerName(container: Container) {
    if (container.l10nId) {
      return ContextualIdentityService.getUserContextLabel(
        container.userContextId,
      );
    }
    return container.name;
  }

  private createFormConfig(workspace: TWorkspace): TForm {
    return {
      forms: [
        {
          id: "name",
          type: "text",
          label: "Name",
          value: workspace.name,
          required: true,
          placeholder: "Enter a name for this workspace",
          classList: "form-control input input-bordered w-full",
        },
        {
          id: "icon",
          type: "select",
          label: "Icon",
          value: workspace.icon || "tab",
          required: true,
          options: this.iconCtx.workspaceIconsArray.map((iconName) => ({
            value: iconName,
            label: iconName,
            icon: this.iconCtx.getWorkspaceIconUrl(iconName),
          })),
          classList: "form-control select select-bordered w-full",
        },
        {
          id: "userContextId",
          type: "select",
          label: "Container",
          value: workspace.userContextId?.toString() || "0",
          required: true,
          options: [
            { value: "0", label: "No Container", icon: "" },
            ...this.containers.map((container) => ({
              value: container.userContextId.toString(),
              label: this.getContainerName(container),
              icon: "",
            })),
          ],
          classList: "form-control select select-bordered w-full",
        },
      ],
      submitLabel: "Save Changes",
      cancelLabel: "Cancel",
    };
  }

  public showWorkspacesModal(workspaceID: TWorkspaceID | null): void {
    const workspace = this.ctx.getRawWorkspace(
      workspaceID ?? this.ctx.getSelectedWorkspaceID(),
    );

    const formConfig = this.createFormConfig(workspace);
    this.modalParent.showNoraModal(formConfig, {
      width: 700,
      height: 600,
    });
  }
}
