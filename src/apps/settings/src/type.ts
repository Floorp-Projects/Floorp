/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";
import { zFloorpDesignConfigs } from "../../../apps/common/scripts/global-types/type";

export const zDesignFormData = z.object({
  design: zFloorpDesignConfigs.shape.globalConfigs.shape.userInterface,
  faviconColor: zFloorpDesignConfigs.shape.globalConfigs.shape.faviconColor,
  style: zFloorpDesignConfigs.shape.tabbar.shape.tabbarStyle,
  position: zFloorpDesignConfigs.shape.tabbar.shape.tabbarPosition,
});

export type DesignFormData = z.infer<typeof zDesignFormData>;
