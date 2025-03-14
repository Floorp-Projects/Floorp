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

  private ContentElement(workspace: TWorkspace) {
    return (
      <div class="w-full max-w-4xl">
        <h1 class="font-bold text-center text-2xl mb-6">Manage Workspaces</h1>
        <div class="flex flex-col gap-6 rounded-box p-8">
          <div class="flex items-center gap-4">
            <i class="fa-solid fa-box text-4xl text-secondary"></i>
            <span class="text-lg">
              Manage your Floorp workspaces. Enter a name for the workspace in
              the Name field, select an icon for the workspace in the Icon
              field, and choose a container in which to display the workspace in
              the Container field.
            </span>
          </div>

          <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
            <div class="space-y-3">
              <label for="name" class="text-sm font-medium">Name</label>
              <input
                type="text"
                id="name"
                class="form-control input input-bordered w-full"
                value={workspace.name}
                placeholder="Enter a name for this workspace"
              />
            </div>

            <div class="space-y-3">
              <label for="iconName" class="text-sm font-medium">Icon</label>
              <div class="dropdown w-full">
                <label
                  tabindex="0"
                  class="flex items-center gap-2 btn btn-outline w-full justify-between hover:bg-base-300"
                >
                  <span class="flex items-center gap-2">
                    <img
                      src={this.iconCtx.getWorkspaceIconUrl(
                        workspace.icon || "tab",
                      )}
                      alt="Selected icon"
                      class="w-4 h-4"
                    />
                    <span>{workspace.icon || "tab"}</span>
                  </span>
                  <i class="fa-solid fa-chevron-down"></i>
                </label>
                <ul
                  tabindex="0"
                  class="dropdown-content menu shadow-lg bg-base-200 rounded-box w-full mt-1 p-2 grid grid-cols-6 gap-2 max-h-[240px] overflow-y-auto"
                >
                  <For each={this.iconCtx.workspaceIconsArray}>
                    {(iconName) => {
                      const iconUrl = this.iconCtx.getWorkspaceIconUrl(
                        iconName,
                      );
                      return (
                        <li class="min-w-[48px]">
                          <button
                            type="button"
                            class={`btn btn-ghost p-2 flex flex-col items-center justify-center hover:bg-base-300 rounded-box transition-all h-auto min-h-0 w-full ${
                              workspace.icon === iconName
                                ? "bg-base-300 border border-primary"
                                : ""
                            }`}
                          >
                            <img
                              src={iconUrl}
                              alt={iconName}
                              width={24}
                              height={24}
                              class="object-contain"
                            />
                            <span class="text-[8px] mt-0.5 truncate w-full text-center leading-tight">
                              {iconName}
                            </span>
                          </button>
                        </li>
                      );
                    }}
                  </For>
                </ul>
              </div>
            </div>

            <div class="space-y-3 md:col-span-2">
              <label for="containerName" class="text-sm font-medium">
                Container
              </label>
              <select
                id="containerName"
                class="form-control select select-bordered w-full"
                value={workspace?.userContextId ?? 0}
              >
                <option value={0}>No Container</option>
                <For each={this.containers}>
                  {(container) => (
                    <option
                      value={container.userContextId}
                      selected={workspace.userContextId ===
                          container.userContextId
                        ? true
                        : undefined}
                    >
                      {this.getContainerName(container)}
                    </option>
                  )}
                </For>
              </select>
            </div>
          </div>

          <div class="flex justify-between items-center w-full mt-4 pt-4 border-t border-base-300">
            <a class="link link-hover link-accent text-sm">
              Learn more
            </a>

            <div class="flex gap-3">
              <button type="button" class="btn btn-ghost">
                Cancel
              </button>

              <button type="button" class="btn btn-primary">
                Save Changes
              </button>
            </div>
          </div>
        </div>
      </div>
    );
  }

  public showWorkspacesModal(workspaceID: TWorkspaceID | null): void {
    const workspace = this.ctx.getRawWorkspace(
      workspaceID ?? this.ctx.getSelectedWorkspaceID(),
    );
    this.modalParent.showNoraModal(
      this.ContentElement(workspace) as Element,
      {
        width: 700,
        height: 600,
      },
    );
  }
}
