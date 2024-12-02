/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, For, Show } from "solid-js";
import { render } from "@nora/solid-xul";
import { ShareModal } from "@core/utils/modal";
import { WorkspaceIcons } from "./utils/workspace-icons.js";
import type { Workspace } from "./utils/type.js";
import { WorkspacesServices } from "./workspaces.js";
import modalStyle from "./modal-style.css?inline";

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  name: string;
  userContextId: number;
  l10nId?: string;
};

export const [workspaceModalState, setWorkspaceModalState] = createSignal<{
  show: boolean;
  targetWorkspace: Workspace | null;
}>({ show: false, targetWorkspace: null });

export class WorkspaceManageModal {
  private static instance: WorkspaceManageModal;
  public static getInstance() {
    if (!WorkspaceManageModal.instance) {
      WorkspaceManageModal.instance = new WorkspaceManageModal();
    }
    return WorkspaceManageModal.instance;
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

  private ContentElement(workspace: Workspace | null) {
    const gWorkspacesServices = WorkspacesServices.getInstance();
    const gWorkspaceIcons = WorkspaceIcons.getInstance();
    const targetWorkspace =
      workspace ?? gWorkspacesServices.getCurrentWorkspace;

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
              <For each={gWorkspaceIcons.workspaceIconsArray}>
                {(icon) => (
                  <xul:menuitem
                    value={icon}
                    label={icon}
                    style={{
                      "list-style-image": `url(${gWorkspaceIcons.getWorkspaceIconUrl(icon)})`,
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
    const gWorkspacesServices = WorkspacesServices.getInstance();
    return (
      <ShareModal
        name="Manage Workspace"
        ContentElement={() =>
          this.ContentElement(workspaceModalState().targetWorkspace)
        }
        StyleElement={() => this.StyleElement}
        onClose={() =>
          setWorkspaceModalState({ show: false, targetWorkspace: null })
        }
        onSave={(formControls) => {
          console.log(formControls);
          const targetWorkspace =
            workspaceModalState().targetWorkspace ??
            gWorkspacesServices.getCurrentWorkspace;
          const newData = {
            ...targetWorkspace,
            name: formControls[0].value,
            icon: formControls[1].value,
            userContextId: Number(formControls[2].value),
          };
          gWorkspacesServices.saveWorkspaceById(targetWorkspace.id, newData);
          setWorkspaceModalState({ show: false, targetWorkspace: null });
        }}
      />
    );
  }

  private WorkspaceManageModal() {
    return <Show when={workspaceModalState().show}>{this.Modal()}</Show>;
  }

  constructor() {
    render(
      () => this.WorkspaceManageModal(),
      document?.getElementById("fullscreen-and-pointerlock-wrapper"),
      {
        // biome-ignore lint/suspicious/noExplicitAny: <explanation>
        hotCtx: (import.meta as any)?.hot,
      },
    );

    render(() => this.StyleElement, document?.head, {
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      hotCtx: (import.meta as any)?.hot,
    });
  }
}
