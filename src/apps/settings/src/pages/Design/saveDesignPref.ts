/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DesignFormData } from "../../type";
import { getStringPref } from "../../dev";
import { zFloorpDesignConfigs } from "../../../../../apps/common/scripts/global-types/type";

export function saveDesignSettings(settings: DesignFormData) {
  console.log(settings);
}

export async function getDesignSettings(): Promise<DesignFormData> {
  /*
  const settings = await getStringPref("floorp.design.configs");
  const parsedSettings = zFloorpDesignConfigs.parse(settings);
  const formDefaultValues = {
    design: parsedSettings.globalConfigs.userInterface ?? "lepton",
    faviconColor: false, // parsedSettings.globalConfigs.faviconColor ?? false,
    style: parsedSettings.tabbar.tabbarStyle ?? "horizontal",
    position: parsedSettings.tabbar.tabbarPosition ?? "default",
  };
  */

  return {
    design: "lepton",
    faviconColor: false,
    style: "horizontal",
    position: "default",
  };

  // return formDefaultValues;
}
