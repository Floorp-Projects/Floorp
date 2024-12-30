/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createEffect, createSignal, onCleanup, Setter } from "solid-js";
import { type TWorkspaces, zWorkspacesServicesStoreData } from "./utils/type.js";
import { WorkspacesServicesStaticNames } from "./utils/workspaces-static-names.js";
import { createRootHMR } from "@nora/solid-xul";

function getWorkspacesServicesArrayData(stringData: string){
  return JSON.parse(stringData).workspaces || [];
}

function createWorkspacesData():[Accessor<TWorkspaces>,Setter<TWorkspaces>]{
  const [workspacesData, setWorkspacesData] = createSignal<TWorkspaces>(
    zWorkspacesServicesStoreData.parse(
      getWorkspacesServicesArrayData(
        Services.prefs.getStringPref(
          WorkspacesServicesStaticNames.workspaceDataPrefName,
          "{}",
        ),
      ),
    ),
  );
  
  createEffect(() => {
    Services.prefs.setStringPref(
      WorkspacesServicesStaticNames.workspaceDataPrefName,
      JSON.stringify({ workspaces: workspacesData() }),
    );
  });

  const observer =() =>
    setWorkspacesData(
      zWorkspacesServicesStoreData.parse(
        getWorkspacesServicesArrayData(
          Services.prefs.getStringPref(
            WorkspacesServicesStaticNames.workspaceDataPrefName,
            "{}",
          ),
        ),
      ),
    )
  Services.prefs.addObserver("floorp.workspaces.v3.data", observer);
  onCleanup(()=>{
    Services.prefs.removeObserver("floorp.workspaces.v3.data", observer);
  })
  return [workspacesData,setWorkspacesData]
}

/** WorkspacesServices data */
export const [workspacesData, setWorkspacesData] = createRootHMR(createWorkspacesData,import.meta.hot);





