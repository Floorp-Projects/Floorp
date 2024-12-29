/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { gFlexOrder } from "./flex-order";

@noraComponent(import.meta.hot)
export default class FlexOrder extends NoraComponentBase {
  init(): void {
    gFlexOrder.init();
  }
}
