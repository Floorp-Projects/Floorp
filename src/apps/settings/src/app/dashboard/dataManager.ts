import { type AccountInfo, type HomeData } from "@/types/pref";

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

export async function getAccountImage(): Promise<string> {
  const accountInfo = await getAccountInfo();
  console.log(accountInfo);
  return accountInfo.status === "not_configured"
    ? "chrome://browser/skin/fxa/avatar-color.svg"
    : (accountInfo.avatarURL ?? "chrome://browser/skin/fxa/avatar-color.svg");
}
