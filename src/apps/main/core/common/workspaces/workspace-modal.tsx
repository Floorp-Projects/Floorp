/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createSignal, For, Setter, Show } from "solid-js";
import { createRootHMR, render } from "@nora/solid-xul";
import { ShareModal } from "@core/utils/modal";
import { WorkspaceIcons } from "./utils/workspace-icons.js";
import type { TWorkspace, TWorkspaceID } from "./utils/type.js";
import { WorkspacesService } from "./workspacesService";
import modalStyle from "./modal-style.css?inline";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data.js";

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
}

export const [workspaceModalState, setWorkspaceModalState] = createRootHMR<[Accessor<ModalState>,Setter<ModalState>]>(()=>createSignal<ModalState>({ show: false, targetWorkspaceID: null }),import.meta.hot);

export class WorkspaceManageModal {
  ctx:WorkspacesService
  iconCtx:WorkspaceIcons
  constructor(ctx:WorkspacesService,iconCtx:WorkspaceIcons) {
    this.ctx=ctx;
    this.iconCtx=iconCtx;
    render(
      () => this.WorkspaceManageModal(),
      document?.getElementById("fullscreen-and-pointerlock-wrapper"),
    );

    render(() => this.StyleElement, document?.head);
  }

  private get containers(): Container[] {
    return ContextualIdentityService.getPublicIdentities();
  }

  private get StyleElement() {
    return <style>{modalStyle}</style>;
  }

  private getContainerName(container: Container) {
    if (container.l10nId) {
      return ContextualIdentityService.getUserContextLabel(
        container.userContextId,
      );
    }
    return container.name;
  }

  private ContentElement(workspace: TWorkspace | null) {
    const targetWorkspace =
      workspace ?? this.ctx.getRawWorkspace(this.ctx.getSelectedWorkspaceID());

    return (
      <>
        <form>
          <label for="name">Name</label>
          <input
            type="text"
            id="name"
            class="form-control"
            value={targetWorkspace.name}
            placeholder="Enter a name for this workspace"
          />
        </form>

        <form>
          <label for="iconName">Icon</label>
          <xul:menulist
            class="form-control"
            flex="1"
            id="iconName"
            value={targetWorkspace.icon ?? "fingerprint"}
          >
            <xul:menupopup id="workspacesIconSelectPopup">
              <For each={this.iconCtx.workspaceIconsArray}>
                {(icon) => (
                  <xul:menuitem
                    value={icon}
                    label={icon}
                    style={{
                      "list-style-image": `url(${this.iconCtx.getWorkspaceIconUrl(icon)})`,
                    }}
                  />
                )}
              </For>
            </xul:menupopup>
          </xul:menulist>
        </form>

        <form>
          <label for="containerName">Container</label>
          <select
            id="containerName"
            class="form-control"
            value={targetWorkspace?.userContextId ?? 0}
          >
            <option value={0}>No Container</option>
            <For each={this.containers}>
              {(container) => (
                <option
                  value={container.userContextId}
                  selected={
                    targetWorkspace.userContextId === container.userContextId
                      ? true
                      : undefined
                  }
                >
                  {this.getContainerName(container)}
                </option>
              )}
            </For>
          </select>
        </form>
      </>
    );
  }

  private Modal() {
    return (
      <ShareModal
        name="Manage Workspace"
        ContentElement={() =>
            this.ContentElement(this.ctx.getRawWorkspace(workspaceModalState().targetWorkspaceID!))
        }
        StyleElement={() => this.StyleElement}
        onClose={() =>
          setWorkspaceModalState({ show: false, targetWorkspaceID: null })
        }
        onSave={(formControls) => {
          console.log(formControls);
          const targetWorkspace =
            workspaceModalState().targetWorkspaceID ??
            this.ctx.getSelectedWorkspaceID()!;
          const newData = {
            ...this.ctx.getRawWorkspace(targetWorkspace),
            name: formControls[0].value,
            icon: formControls[1].value,
            userContextId: Number(formControls[2].value),
          };
          setWorkspacesDataStore("data",(prev)=>{prev.set(targetWorkspace,newData);return new Map(prev);})
          setWorkspaceModalState({ show: false, targetWorkspaceID: null });
        }}
      />
    );
  }

  private WorkspaceManageModal() {
    return <Show when={workspaceModalState().show}>{this.Modal()}</Show>;
  }
}
