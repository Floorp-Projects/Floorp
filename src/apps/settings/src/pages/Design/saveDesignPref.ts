/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DesignFormData } from "../../type";
import { zFloorpDesignConfigs } from "../../../../../apps/common/scripts/global-types/type";

export function saveDesignSettings(settings: DesignFormData) {
  console.log(settings);
}

export async function getDesignSettings(): Promise<DesignFormData> {
  return await new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.design.configs",
        prefType: "string",
      },
      (stringData: string) => {
        const data = zFloorpDesignConfigs.parse(
          JSON.parse(JSON.parse(stringData).prefValue),
        );
        const formData: DesignFormData = {
          design: data.globalConfigs.userInterface,
          faviconColor: true,
          position: data.tabbar.tabbarPosition,
          style: data.tabbar.tabbarStyle,
        };
        resolve(formData);
      },
    );
  });
}
