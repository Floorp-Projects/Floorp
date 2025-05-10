import { noraComponent, NoraComponentBase } from "@core/utils/base.ts";
import { WorkspacesTabManager } from "./workspacesTabManager.tsx";
import { WorkspaceIcons } from "./utils/workspace-icons.ts";
import { WorkspacesService } from "./workspacesService.ts";
import { WorkspaceManageModal } from "./workspace-modal.tsx";
import { WorkspacesToolbarButton } from "./toolbar/toolbar-element.tsx";
import { WorkspacesPopupContxtMenu } from "./contextMenu/popupSet.tsx";
import { WorkspacesDataManager } from "./workspacesDataManagerBase.tsx";
import { enabled } from "@core/common/workspaces/data/config.ts";
import { WorkspacesTabContextMenu } from "./tabContextMenu.tsx";

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static windowWorkspacesMap: WeakMap<Window, WorkspacesService> =
    new WeakMap();

  static getCtx(): WorkspacesService | null {
    if (!window || !enabled()) {
      return null;
    }
    return this.windowWorkspacesMap.get(window) || null;
  }

  init(): void {
    if (!enabled()) {
      return;
    }

    const iconCtx = new WorkspaceIcons();
    const dataManagerCtx = new WorkspacesDataManager();
    const tabCtx = new WorkspacesTabManager(iconCtx, dataManagerCtx);
    const ctx = new WorkspacesService(tabCtx, iconCtx, dataManagerCtx);
    Workspaces.windowWorkspacesMap.set(window, ctx);
    new WorkspaceManageModal(ctx, iconCtx);
    new WorkspacesToolbarButton(ctx);
    new WorkspacesPopupContxtMenu(ctx);
    new WorkspacesTabContextMenu(ctx);
  }
}
