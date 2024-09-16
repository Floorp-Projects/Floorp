/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { z } from "zod";
import { config, type zFloorpDesignConfigs } from "../../designs/configs";
import { For } from "solid-js";
import verticalTabarStyle from "./vertical-tabbar.css?inline";
import verticalTabarHoverStyle from "./vertical-tabbar-hover.css?inline";
import verticalTabarPaddingStyle from "./vertical-tabbar-padding.css?inline";

function getVerticalTabbarCSSFromConfig(
  pref: z.infer<typeof zFloorpDesignConfigs>,
): string[] {
  const result: string[] = [];

  if (pref.tabbar.tabbarStyle === "vertical") {
    result.push(verticalTabarStyle);

    if (pref.tabbar.verticalTabBar.hoverEnabled) {
      result.push(verticalTabarHoverStyle);
    }
    if (pref.tabbar.verticalTabBar.paddingEnabled) {
      result.push(verticalTabarPaddingStyle);
    }
    //console.log(pref.tabbar.verticalTabBar);
  }
  return result;
}

export function VerticalTabbarStyle() {
  return (
    <For each={getVerticalTabbarCSSFromConfig(config())}>
      {(style) => <style>{style}</style>}
    </For>
  );
}
