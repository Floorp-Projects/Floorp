/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import { designConfigs } from "./configs";
import leptonChromeStyles from "./styles/lepton/css/leptonChrome.css?url";
import leptonTabStyles from "./styles/lepton/css/leptonContent.css?url";
import fluerialStyles from "./styles/fluerial/css/fluerial.css?url";

const browserDesignStyles: {
  [key: string]: { [key: number]: string[] };
} = {
  designs: {
    0: [leptonChromeStyles, leptonTabStyles],
    3: [fluerialStyles],
  },
  verticalTabs: {
    0: [],
    3: [],
  },
  multiRowTabs: {
    0: [],
    3: [],
  },
};

export function BrowserDesignElement() {
  return (
    <>
      <For
        each={
          browserDesignStyles.designs[designConfigs.globalConfigs.userInterface]
        }
      >
        {(style) => (
          <link rel="stylesheet" class="browser-design" href={style} />
        )}
      </For>
    </>
  );
}
