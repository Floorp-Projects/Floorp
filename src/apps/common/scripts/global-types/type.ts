/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

export type zFloorpDesignConfigsType = z.infer<typeof zFloorpDesignConfigs>;

export const zFloorpDesignConfigs = z.object({
  globalConfigs: z.object({
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
    verticalTabBar: z.object({
      hoverEnabled: z.boolean(),
      paddingEnabled: z.boolean(),
      width: z.number(),
    }),
    tabScroll: z.object({
      reverse: z.boolean(),
      wrap: z.number(),
    }),
  }),
  fluerial: z.object({
    roundVerticalTabs: z.boolean(),
  }),
});
