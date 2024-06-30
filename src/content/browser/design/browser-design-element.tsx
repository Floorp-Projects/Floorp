/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import type { z } from "zod";
import type { zFloorpDesignConfigs } from "./configs";
import { config } from "./configs";
import leptonChromeStyles from "./styles/lepton/css/leptonChrome.css?url";
import leptonTabStyles from "./styles/lepton/css/leptonContent.css?url";
import fluerialStyles from "./styles/fluerial/css/fluerial.css?url";

function getCSSFromConfig(
  pref: z.infer<typeof zFloorpDesignConfigs>,
): string[] {
  const result: string[] = [];
  switch (pref.globalConfigs.userInterface) {
    case "fluerial": {
      result.push(fluerialStyles);
      break;
    }
    case "lepton": {
      [leptonChromeStyles, leptonTabStyles].forEach((style) => {
        result.push(style);
      });
    }
  }
  return result;
}

export function BrowserDesignElement() {
  return (
    <>
      <For each={getCSSFromConfig(config())}>
        {(style) => (
          <link rel="stylesheet" class="browser-design" href={style} />
        )}
      </For>
    </>
  );
}
