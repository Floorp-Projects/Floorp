/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, onCleanup } from "solid-js";
import { TWorkspacesStoreData, zWorkspaceID, zWorkspacesServicesStoreData } from "../utils/type.js";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.js";
import { createRootHMR } from "@nora/solid-xul";
import {createStore, SetStoreFunction, Store, unwrap} from "solid-js/store"

function getDefaultStore() {
  const result = zWorkspacesServicesStoreData.safeParse(
    JSON.parse(
      Services.prefs.getStringPref(
        WORKSPACE_DATA_PREF_NAME,
        "{}",
      ),
    ),
  );
  if (result.success) {
    return result.data
  } else {
    const stubID = zWorkspaceID.parse("00000000-0000-0000-0000-000000000000");
    return {
      defaultID: stubID,
      selectedID: stubID,
      data: new Map(),
      order: []
    } satisfies TWorkspacesStoreData
  }
}

function createWorkspacesData(): [Store<TWorkspacesStoreData>,SetStoreFunction<TWorkspacesStoreData>] {
  const [workspacesDataStore,setWorkspacesDataStore] = createStore(getDefaultStore());

  createEffect(() => {
      Services.prefs.setStringPref(
        WORKSPACE_DATA_PREF_NAME,
        JSON.stringify(unwrap(workspacesDataStore)),
      );
  });

  const observer = () => {
    const result = zWorkspacesServicesStoreData.safeParse(
      JSON.parse(
        Services.prefs.getStringPref(
          WORKSPACE_DATA_PREF_NAME,
          "{}",
        ),
      ),
    )
    if (result.success) {
      const _storedData = result.data;
      setWorkspacesDataStore("data",_storedData.data);
      setWorkspacesDataStore("defaultID",_storedData.defaultID);
      setWorkspacesDataStore("selectedID",_storedData.selectedID)
    }
  }
  Services.prefs.addObserver(WORKSPACE_DATA_PREF_NAME, observer);
  onCleanup(()=>{
    Services.prefs.removeObserver(WORKSPACE_DATA_PREF_NAME, observer);
  })
  return [workspacesDataStore,setWorkspacesDataStore];
}

/** WorkspacesServices data */
export const [workspacesDataStore,setWorkspacesDataStore] = createRootHMR(createWorkspacesData,import.meta.hot);

