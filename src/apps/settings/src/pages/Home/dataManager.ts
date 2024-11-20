/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { HomeData, AccountInfo } from "@/type";

export async function useHomeData(): Promise<HomeData> {
  return {
    accountName: (await getAccountName()) ?? null,
    accountImage: await getAccountImage(),
  };
}

export async function getAccountInfo(): Promise<AccountInfo> {
  return await new Promise((resolve) => {
    window.NRGetAccountInfo((data: string) => {
      const accountInfo = JSON.parse(data) as AccountInfo;
      resolve(accountInfo);
    });
  });
}

async function getAccountName(): Promise<string | null> {
  const accountInfo = await getAccountInfo();
  return accountInfo.status === "not_configured"
    ? null
    : (accountInfo.displayName ?? accountInfo.email);
}

async function getAccountImage(): Promise<string> {
  const accountInfo = await getAccountInfo();
  return accountInfo.status === "not_configured"
    ? "chrome://browser/skin/fxa/avatar-color.svg"
    : (accountInfo.avatarURL ?? "chrome://browser/skin/fxa/avatar-color.svg");
}
