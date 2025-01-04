import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { WorkspacesTabManager } from "./workspacesTabManager";
import { WorkspaceIcons } from "./utils/workspace-icons";
import { WorkspacesService } from "./workspacesService";
import { WorkspaceManageModal } from "./workspace-modal";
import { WorkspacesToolbarButton } from "./toolbar/toolbar-element";
import { WorkspacesPopupContxtMenu } from "./popup/popupSet";

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static ctx: WorkspacesService | null = null;
  init(): void {
    const iconCtx = new WorkspaceIcons();
    const tabCtx = new WorkspacesTabManager(iconCtx);
    const ctx = new WorkspacesService(tabCtx,iconCtx)
    new WorkspaceManageModal(ctx,iconCtx);
    new WorkspacesToolbarButton(ctx);
    new WorkspacesPopupContxtMenu(ctx);
    Workspaces.ctx = ctx;
  }
}