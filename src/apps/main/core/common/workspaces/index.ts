import { noraComponent, NoraComponentBase } from "@core/utils/base.ts";
import { WorkspacesTabManager } from "./workspacesTabManager.tsx";
import { WorkspaceIcons } from "./utils/workspace-icons.ts";
import { WorkspacesService } from "./workspacesService.ts";
import { WorkspaceManageModal } from "./workspace-modal.tsx";
import { WorkspacesToolbarButton } from "./toolbar/toolbar-element.tsx";
import { WorkspacesPopupContxtMenu } from "./contextMenu/popupSet.tsx";
import { WorkspacesDataManager } from "./workspacesDataManagerBase.tsx";

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static ctx: WorkspacesService | null = null;
  init(): void {
    const iconCtx = new WorkspaceIcons();
    const dataManagerCtx = new WorkspacesDataManager();
    const tabCtx = new WorkspacesTabManager(iconCtx, dataManagerCtx);
    const ctx = new WorkspacesService(tabCtx, iconCtx, dataManagerCtx);
    new WorkspaceManageModal(ctx, iconCtx);
    new WorkspacesToolbarButton(ctx);
    new WorkspacesPopupContxtMenu(ctx);
    Workspaces.ctx = ctx;
  }
}
