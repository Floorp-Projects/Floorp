/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { WorkspacesFormData } from "@/type";

export async function saveWorkspaceSettings(settings: WorkspacesFormData) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfigs = await getWorkspaceSettings();

  const newConfigs = {
    ...oldConfigs,
    ...settings,
  };

  return await new Promise<boolean>((resolve) => {
    Promise.all([
      window.NRSPrefSet(
        {
          prefName: "floorp.workspaces.configs",
          prefValue: JSON.stringify(newConfigs),
          prefType: "string",
        },
        () => {},
      ),
      window.NRSPrefSet(
        {
          prefName: "floorp.browser.workspaces.enabled",
          prefValue: newConfigs.enabled ?? true,
          prefType: "boolean",
        },
        () => {},
      ),
    ]).then(() => resolve(true));
  });
}

export async function getWorkspaceSettings(): Promise<WorkspacesFormData> {
  const [enabled, configs] = await Promise.all([
    await workspacesEnabled(),
    await getWorkspacesConfigsExcludeEnabled(),
  ]);

  return {
    ...configs,
    enabled: enabled,
  };
}

async function workspacesEnabled(): Promise<boolean> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.browser.workspaces.enabled",
        prefType: "boolean",
      },
      (data: string) => {
        const parsedData = JSON.parse(data);
        resolve(parsedData.prefValue);
      },
    );
  });
}

async function getWorkspacesConfigsExcludeEnabled(): Promise<
  Omit<WorkspacesFormData, "enabled">
> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.workspaces.configs",
        prefType: "string",
      },
      (data: string) => {
        const parsedData = JSON.parse(data);
        const configs = JSON.parse(parsedData.prefValue);
        resolve(configs);
      },
    );
  });
}
