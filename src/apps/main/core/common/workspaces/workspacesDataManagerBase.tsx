import { TWorkspace } from "./utils/type";
import { workspacesDataStore, setWorkspacesDataStore } from "./data/data";
import { WorkspaceID } from "./utils/type";

export class WorkspacesDataManagerBase {
    constructor() {
      
    }
    /**
     * Returns new workspace object.
     * @param name The name of the workspace.
     * @returns The new workspace id.
     */
    public createWorkspace(name: string): WorkspaceID {
      const id = Services.uuid.generateUUID().toString();
      const workspace: TWorkspace = {
        name,
        icon: null,
        userContextId: 0,
      };
      //TODO: If the id is duplicated, regenerate with limit to try
      setWorkspacesDataStore("data",(prev)=>{
        prev.set(id,workspace)
        return new Map(prev);
      })
      return id as WorkspaceID;
    }

    /**
     * Delete workspace by id.
     * @param workspaceId The workspace id.
     */
    public deleteWorkspace(workspaceID: WorkspaceID): void {
      this.setCurrentWorkspaceID(
        workspacesDataStore.defaultID as WorkspaceID,
      );

      setWorkspacesDataStore("data",(prev)=>{
        prev.delete(workspaceID)
        return new Map(prev)
      });
    }

    public setCurrentWorkspaceID(workspaceID: WorkspaceID): void {
      setWorkspacesDataStore("selectedID",workspaceID)
    }

    public setDefaultWorkspace(workspaceID:WorkspaceID): void {
      setWorkspacesDataStore("defaultID",workspaceID)
    }
}