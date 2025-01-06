import { TWorkspace, zWorkspaceID } from "./utils/type";
import { workspacesDataStore, setWorkspacesDataStore } from "./data/data";
import { TWorkspaceID } from "./utils/type";

export interface WorkspacesDataManagerBase {
  createWorkspace(name:string):TWorkspaceID
  deleteWorkspace(id: TWorkspaceID): void
  setCurrentWorkspaceID(id: TWorkspaceID): void
  setDefaultWorkspace(id:TWorkspaceID): void
  isWorkspaceID(id: string): id is TWorkspaceID
  getRawWorkspace(id: TWorkspaceID): TWorkspace
  getSelectedWorkspaceID(): TWorkspaceID
  getDefaultWorkspaceID(): TWorkspaceID
}

export class WorkspacesDataManager implements WorkspacesDataManagerBase {
    /**
     * Returns new workspace object.
     * @param name The name of the workspace.
     * @returns The new workspace id.
     * @deprecated Use WorkspacesServices.createWorkspace
     */
    public createWorkspace(name: string): TWorkspaceID {
      const id = zWorkspaceID.parse(Services.uuid.generateUUID().toString().replaceAll(/[{}]/g,""));
      const workspace: TWorkspace = {
        name,
        icon: null,
        userContextId: 0,
      };
      //TODO: If the id is duplicated, regenerate with limit to try
      setWorkspacesDataStore("data",(prev)=>{
        prev.set(id,workspace)
        return new Map(prev);
      });
      return id;
    }

    /**
     * Delete workspace by id.
     * @param workspaceId The workspace id.
     */
    public deleteWorkspace(id: TWorkspaceID): void {
      this.setCurrentWorkspaceID(
        workspacesDataStore.defaultID as TWorkspaceID,
      );

      setWorkspacesDataStore("data",(prev)=>{
        prev.delete(id)
        return new Map(prev)
      });
    }

    public setCurrentWorkspaceID(id: TWorkspaceID): void {
      setWorkspacesDataStore("selectedID",id)
    }

    public setDefaultWorkspace(id:TWorkspaceID): void {
      setWorkspacesDataStore("defaultID",id)
    }

    public isWorkspaceID(id: string): id is TWorkspaceID {
      return workspacesDataStore.data.has(zWorkspaceID.parse(id));
    }

    public getRawWorkspace(id: TWorkspaceID): TWorkspace {
      return workspacesDataStore.data.get(id)!;
    }

    public getSelectedWorkspaceID(): TWorkspaceID {
      if (this.isWorkspaceID(workspacesDataStore.selectedID)){
        return workspacesDataStore.selectedID
      }
      throw Error("Not Valid Selected Workspace ID : "+workspacesDataStore.selectedID)
    }

    public getDefaultWorkspaceID(): TWorkspaceID {
      if(this.isWorkspaceID(workspacesDataStore.defaultID)) {
        return workspacesDataStore.defaultID;
      }
      throw Error("Not Valid Default Workspace ID : "+workspacesDataStore.defaultID)
    }
}