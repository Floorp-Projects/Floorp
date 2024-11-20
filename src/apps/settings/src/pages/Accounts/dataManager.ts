/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getAccountInfo } from "@/pages/Home/dataManager";
import type { AccountsFormData } from "@/type";

export async function useAccountAndProfileData(): Promise<AccountsFormData> {
  const homeData = await getAccountInfo();
  return {
    accountInfo: homeData,
    profileDir: "テスト",
    profileName: "テスト",
    asyncNoesViaMozillaAccount: true,
  };
}
