/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { type Workspaces, zWorkspacesServicesStoreData } from "./utils/type.js";
import { WorkspacesServicesStaticNames } from "./utils/workspaces-static-names.js";

/** WorkspacesServices data */
export const [workspacesData, setworkspacesData] = createSignal<Workspaces>(
  zWorkspacesServicesStoreData.parse(
    getworkspacesServicesArrayData(
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

Services.prefs.addObserver("floorp.workspaces.v3.data", () =>
  setworkspacesData(
    zWorkspacesServicesStoreData.parse(
      getworkspacesServicesArrayData(
        Services.prefs.getStringPref(
          WorkspacesServicesStaticNames.workspaceDataPrefName,
          "{}",
        ),
      ),
    ),
  ),
);

function getworkspacesServicesArrayData(stringData: string) {
  return JSON.parse(stringData).workspaces || [];
}
