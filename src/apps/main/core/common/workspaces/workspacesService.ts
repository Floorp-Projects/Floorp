import { onCleanup } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
import { WorkspaceID } from "./utils/type";
import { setWorkspaceModalState } from "./workspace-modal";
import { WorkspacesDataManagerBase } from "./workspacesDataManagerBase";
import { WorkspacesTabManager } from "./workspacesTabManager";
import { WorkspaceIcons } from "./utils/workspace-icons";


export class WorkspacesService extends WorkspacesDataManagerBase {
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons
  constructor(tabManagerCtx:WorkspacesTabManager, iconCtx: WorkspaceIcons){
    super();
    if (workspacesDataStore.data.size === 0) {
      const id = this.createNoNameWorkspace();
      this.setDefaultWorkspace(id);
      this.setCurrentWorkspaceID(id);
    }
    this.tabManagerCtx = tabManagerCtx;
    this.iconCtx = iconCtx;

    window.gBrowser.addProgressListener(this.listener);

    onCleanup(()=>{
      window.gBrowser.removeProgressListener(this.listener)
    })
  }

  override deleteWorkspace(workspaceID: WorkspaceID): void {
    if (workspacesDataStore.data.size === 1) return;
    this.tabManagerCtx.removeTabByWorkspaceId(workspaceID);
    super.deleteWorkspace(workspaceID)
  }

  /**
     * Returns new workspace object with default name.
     * @returns The new workspace id.
     */
  public createNoNameWorkspace(): WorkspaceID {
    //TODO: i18n support
    return this.createWorkspace(
      `New Workspaces (${workspacesDataStore.data.size})`,
    );
  };

  override createWorkspace(name: string): WorkspaceID {
    const id = super.createWorkspace(name);
    setWorkspacesDataStore("order",(prev)=>[...prev,id])
    return id;
  }

  reorderWorkspaceUp(id: WorkspaceID): void {
    setWorkspacesDataStore("order",(prev)=>{
      const index = prev.indexOf(id);
      if (index > 0) [prev[index-1],prev[index]] = [prev[index],prev[index-1]];
      return [...prev]
    })
  }

  reorderWorkspaceDown(id: WorkspaceID): void {
    setWorkspacesDataStore("order",(prev)=>{
      const index = prev.indexOf(id);
      if (index < prev.length-1) [prev[index],prev[index+1]] = [prev[index+1],prev[index]];
      return [...prev]
    })
  }

  /**
   * Open manage workspace dialog. This function should not be called directly on Preferences page.
   * @param workspaceId If workspaceId is provided, the dialog will select the workspace for editing.
   */
  public manageWorkspaceFromDialog(id?: WorkspaceID) {
    const targetWorkspaceID = id ?? workspacesDataStore.selectedID as WorkspaceID;
    setWorkspaceModalState({ show: true, targetWorkspaceID });
  }

  public changeWorkspace(id: WorkspaceID) {
    this.tabManagerCtx.changeWorkspace(id);
  }

  /**
   * Location Change Listener.
   */
  private listener = {
    /**
     * Listener for location change. This function will monitor the location change and check the tabs visibility.
     * @returns void
     */
    onLocationChange: () => {
      this.tabManagerCtx.updateTabsVisibility();
    },
  };

}