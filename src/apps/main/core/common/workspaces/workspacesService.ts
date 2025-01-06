import { onCleanup } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
import { TWorkspace, TWorkspaceID } from "./utils/type";
import { setWorkspaceModalState } from "./workspace-modal";
import { WorkspacesDataManager, WorkspacesDataManagerBase } from "./workspacesDataManagerBase";
import { WorkspacesTabManager } from "./workspacesTabManager";
import { WorkspaceIcons } from "./utils/workspace-icons";


export class WorkspacesService implements WorkspacesDataManagerBase {
  dataManagerCtx: WorkspacesDataManager;
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons
  constructor(tabManagerCtx:WorkspacesTabManager, iconCtx: WorkspaceIcons, dataManagerCtx: WorkspacesDataManager){
    this.tabManagerCtx = tabManagerCtx;
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    if (workspacesDataStore.data.size === 0) {
      const id = this.createNoNameWorkspace();
      this.setDefaultWorkspace(id);
      this.setCurrentWorkspaceID(id);
    }

    window.gBrowser.addProgressListener(this.listener);

    onCleanup(()=>{
      window.gBrowser.removeProgressListener(this.listener)
    })
  }
  setCurrentWorkspaceID(id: TWorkspaceID): void {
    this.dataManagerCtx.setCurrentWorkspaceID(id)
  }
  setDefaultWorkspace(id: TWorkspaceID): void {
    this.dataManagerCtx.setDefaultWorkspace(id)
  }
  isWorkspaceID(id: string): id is TWorkspaceID {
    return this.dataManagerCtx.isWorkspaceID(id)
  }
  getRawWorkspace(id: TWorkspaceID): TWorkspace {
    return this.dataManagerCtx.getRawWorkspace(id)
  }
  getSelectedWorkspaceID(): TWorkspaceID {
    return this.dataManagerCtx.getSelectedWorkspaceID()
  }
  getDefaultWorkspaceID(): TWorkspaceID {
    return this.dataManagerCtx.getDefaultWorkspaceID()
  }

  deleteWorkspace(workspaceID: TWorkspaceID): void {
    if (workspacesDataStore.data.size === 1) return;
    this.tabManagerCtx.removeTabByWorkspaceId(workspaceID);
    setWorkspacesDataStore("order",(prev)=>prev.filter((v)=>v !== workspaceID))
    this.dataManagerCtx.deleteWorkspace(workspaceID)
  }

  /**
     * Returns new workspace object with default name.
     * @returns The new workspace id.
     */
  public createNoNameWorkspace(): TWorkspaceID {
    //TODO: i18n support
    return this.createWorkspace(
      `New Workspaces (${workspacesDataStore.data.size})`,
    );
  };

  createWorkspace(name: string): TWorkspaceID {
    //This function is deprecated because WorkspacesService is wrapping this function
    const id = this.dataManagerCtx.createWorkspace(name);
    setWorkspacesDataStore("order",(prev)=>[...prev,id])
    return id;
  }

  reorderWorkspaceUp(id: TWorkspaceID): void {
    setWorkspacesDataStore("order",(prev)=>{
      const index = prev.indexOf(id);
      if (index > 0) [prev[index-1],prev[index]] = [prev[index],prev[index-1]];
      return [...prev]
    })
  }

  reorderWorkspaceDown(id: TWorkspaceID): void {
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
  public manageWorkspaceFromDialog(id?: TWorkspaceID) {
    const targetWorkspaceID = id ?? this.getSelectedWorkspaceID();
    setWorkspaceModalState({ show: true, targetWorkspaceID });
  }

  public changeWorkspace(id: TWorkspaceID) {
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