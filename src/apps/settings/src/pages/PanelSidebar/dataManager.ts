/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { PanelSidebarFormData } from "@/type";

/** save panel sidebar settings */
export async function savePanelSidebarSettings(settings: PanelSidebarFormData) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfigs = await getPanelSidebarSettings();

  const newConfigs = {
    ...oldConfigs,
    ...settings,
  };

  const { enabled, ...configs } = newConfigs;

  return await new Promise<boolean>((resolve) => {
    Promise.all([
      window.NRSPrefSet(
        {
          prefName: "floorp.panelSidebar.config",
          prefValue: JSON.stringify(configs),
          prefType: "string",
        },
        () => {},
      ),
      window.NRSPrefSet(
        {
          prefName: "floorp.panelSidebar.enabled",
          prefValue: enabled ?? true,
          prefType: "boolean",
        },
        () => {},
      ),
    ]).then(() => resolve(true));
  });
}

/** get panel sidebar settings */
export async function getPanelSidebarSettings(): Promise<PanelSidebarFormData> {
  const [enabled, configs] = await Promise.all([
    await panelSidebarEnabled(),
    await getPanelSidebarConfigsExcludeEnabled(),
  ]);

  return {
    ...configs,
    enabled: enabled,
  };
}

/** get panel sidebar enabled */
async function panelSidebarEnabled(): Promise<boolean> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.panelSidebar.enabled",
        prefType: "boolean",
      },
      (data: string) => {
        const parsedData = JSON.parse(data);
        resolve(parsedData.prefValue);
      },
    );
  });
}

async function getPanelSidebarConfigsExcludeEnabled(): Promise<
  Omit<PanelSidebarFormData, "enabled">
> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.panelSidebar.config",
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
