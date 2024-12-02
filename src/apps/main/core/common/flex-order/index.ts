/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { gFlexOrder } from "./flex-order";

export async function init() {
  gFlexOrder.init();

  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (import.meta as any).hot?.accept(async (m: any) => {
    await m?.init();
  });
}
