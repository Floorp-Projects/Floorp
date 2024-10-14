/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DesignFormData } from "../../type";
import { zFloorpDesignConfigs } from "../../../../../apps/common/scripts/global-types/type";

export async function saveDesignSettings(settings: DesignFormData) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  return await new Promise((resolve) => {
    window.NRSPrefGet(
      {
        prefName: "floorp.design.configs",
        prefType: "string",
      },
      (stringData: string) => {
        const oldData = zFloorpDesignConfigs.parse(
          JSON.parse(JSON.parse(stringData).prefValue),
        );

        const newData = {
          ...oldData,
          globalConfigs: {
            ...oldData.globalConfigs,
            userInterface: settings.design,
          },
          tabbar: {
            ...oldData.tabbar,
            tabbarPosition: settings.position,
            tabbarStyle: settings.style,
          },
          tab: {
            ...oldData.tab,
            tabbarScroll: {
              ...oldData.tab.tabbarScroll,
              reverse: settings.tabScrollReverse,
              wrap: settings.tabScrollWrap,
            },
            tabbarOpenPosition: settings.tabOpenPosition,
            tabbarMinHeight: settings.tabMinHeight,
            tabbarMinWidth: settings.tabMinWidth,
            tabbarPinTitle: settings.tabPinTitle,
            tabDubleClickToClose: settings.tabDubleClickToClose,
          },
        };

        window.NRSPrefSet(
          {
            prefType: "string",
            prefName: "floorp.design.configs",
            prefValue: JSON.stringify(newData),
          },
          (d: unknown) => console.log(d),
        );
        resolve(true);
      },
    );
  });
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
          tabOpenPosition: data.tab.tabOpenPosition,
          tabMinHeight: data.tab.tabbarMinHeight,
          tabMinWidth: data.tab.tabbarMinWidth,
          tabPinTitle: data.tab.tabbarPinTitle,
          tabScrollReverse: data.tab.tabbarScroll.reverse,
          tabScrollWrap: data.tab.tabbarScroll.wrap,
          tabDubleClickToClose: data.tab.tabDubleClickToClose,
          tabScroll: data.tab.tabbarScroll.enable,
        };
        resolve(formData);
      },
    );
  });
}
