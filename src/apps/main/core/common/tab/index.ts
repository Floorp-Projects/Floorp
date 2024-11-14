/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TabScroll } from "./scroll";
import { TabOpenPosition } from "./openPosition";
import { TabSizeSpecification } from "./sizeSpecification";
import { TabDoubleClickClose } from "./doubleClickClose";
import { TabPinnedTabCustomization } from "./pinnedTabCustomization";

export function init() {
  TabScroll.getInstance();
  TabOpenPosition.getInstance();
  TabSizeSpecification.getInstance();
  TabDoubleClickClose.getInstance();
  TabPinnedTabCustomization.getInstance();

  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (import.meta as any).hot?.accept((m: { init: () => void }) => {
    m?.init();
  });
}
