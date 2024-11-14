/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ConstantsData } from "@/type";

export async function useConstantsData(): Promise<ConstantsData> {
  return await getConstants();
}

async function getConstants(): Promise<ConstantsData> {
  return await new Promise((resolve) => {
    window.NRGetConstants((data: string) => {
      resolve(JSON.parse(data));
    });
  });
}
