/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

/** Design configs */
export const zFloorpDesignConfigs = z.object({
  globalConfigs: z.object({
    faviconColor: z.boolean(),
    userInterface: z.enum([
      "fluerial",
      "lepton",
      "photon",
      "protonfix",
      "proton",
    ]),
    appliedUserJs: z.string(),
  }),
  tabbar: z.object({
    tabbarStyle: z.enum(["horizontal", "vertical", "multirow"]),
    paddingEnabled: z.boolean(),
    tabbarPosition: z.enum([
      "hide-horizontal-tabbar",
      "optimise-to-vertical-tabbar",
      "bottom-of-navigation-toolbar",
      "bottom-of-window",
      "default",
    ]),
    multiRowTabBar: z.object({
      maxRowEnabled: z.boolean(),
      maxRow: z.number(),
    }),
  }),
  tab: z.object({
    tabScroll: z.object({
      enabled: z.boolean(),
      reverse: z.boolean(),
      wrap: z.boolean(),
    }),
    tabMinHeight: z.number(),
    tabMinWidth: z.number(),
    tabPinTitle: z.boolean(),
    tabDubleClickToClose: z.boolean(),
    tabOpenPosition: z.number(),
  }),
});

export type zFloorpDesignConfigsType = z.infer<typeof zFloorpDesignConfigs>;

/** Panel sidebar configs */
export const zPanelSidebarConfigs = z.object({
  enabled: z.boolean(),
});

export type zPanelSidebarConfigsType = z.infer<typeof zPanelSidebarConfigs>;
