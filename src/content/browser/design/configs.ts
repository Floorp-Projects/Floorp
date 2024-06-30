/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

export function getOldConfigs() {
  const oldConfigs = {
    globalConfigs: {
      verticalTabEnabled: false,
      multiRowTabEnabled: false,
      userInterface: 0,
    },
    fluerial: {
      roundVerticalTabs: false,
    },
  };

  Services.prefs.setStringPref(
    "floorp.design.configs",
    JSON.stringify(oldConfigs),
  );
  return JSON.stringify(oldConfigs);
}

export const zFloorpDesignConfigs = z.object({
  globalConfigs: z.object({
    verticalTabEnabled: z.boolean(),
    multiRowTabEnabled: z.boolean(),
    userInterface: z.number(),
  }),
  fluerial: z.object({
    roundVerticalTabs: z.boolean(),
  }),
});

export const designConfigs = zFloorpDesignConfigs.parse(
  JSON.parse(
    Services.prefs.getStringPref("floorp.design.configs", getOldConfigs()),
  ),
);
