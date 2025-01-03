/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TProgressiveWebAppFormData, TProgressiveWebAppObject } from "@/type";
import type { TPwaConfig } from "../../../../main/core/common/pwa/type";

/** save progressive web app settings */
export async function saveProgressiveWebAppSettings(
  settings: TProgressiveWebAppFormData,
) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfigs = await getProgressiveWebAppConfigs();

  const newConfigs = {
    ...oldConfigs,
    ...settings,
  };

  const { enabled, ...configs } = newConfigs;

  return await new Promise<boolean>((resolve) => {
    Promise.all([
      window.NRSPrefSet(
        {
          prefName: "floorp.browser.ssb.config",
          prefValue: JSON.stringify(configs),
          prefType: "string",
        },
        () => {},
      ),
      window.NRSPrefSet(
        {
          prefName: "floorp.browser.ssb.enabled",
          prefValue: enabled,
          prefType: "boolean",
        },
        () => {},
      ),
    ]).then(() => resolve(true));
  });
}

/** get progressive web app settings and installed apps */
export async function getProgressiveWebAppSettingsAndInstalledApps(): Promise<TProgressiveWebAppFormData> {
  const [enabled, configs] = await Promise.all([
    progressiveWebAppEnabled(),
    getProgressiveWebAppConfigs(),
  ]);

  return {
    enabled: enabled,
    ...configs,
  };
}

/** get progressive web app installed apps */
export async function getProgressiveWebAppInstalledApps(): Promise<TProgressiveWebAppObject> {
  return new Promise((resolve) => {
    window.NRGetInstalledApps((installedApps: string) => {
      resolve(JSON.parse(installedApps));
    });
  });
}

/** get panel sidebar enabled */
async function progressiveWebAppEnabled(): Promise<boolean> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.browser.ssb.enabled",
        prefType: "boolean",
      },
      (data: string) => {
        const parsedData = JSON.parse(data);
        resolve(parsedData.prefValue);
      },
    );
  });
}

/** get progressive web app configs */
async function getProgressiveWebAppConfigs(): Promise<TPwaConfig> {
  return new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.browser.ssb.config",
        prefType: "string",
      },
      (data: string) => {
        const parsedData = JSON.parse(JSON.parse(data).prefValue);
        console.log(parsedData);
        resolve(parsedData);
      },
    );
  });
}
