import { getAccountImage, getAccountInfo } from "@/app/dashboard/dataManager";
import type { AccountsFormData } from "@/types/pref";

export async function getCurrentProfile(): Promise<{
  profileName: string;
  profilePath: string;
}> {
  return await new Promise((resolve) => {
    window.NRGetCurrentProfile((data: string) => {
      const profileInfo = JSON.parse(data) as {
        profileName: string;
        profilePath: string;
      };
      resolve(profileInfo);
    });
  });
}

export async function useAccountAndProfileData(): Promise<AccountsFormData> {
  const accountInfo = await getAccountInfo();
  const accountImage = await getAccountImage();
  const profileInfo = await getCurrentProfile();

  console.log("accountInfo", profileInfo);

  return {
    accountInfo: accountInfo,
    accountImage: accountImage,
    profileDir: profileInfo.profilePath,
    profileName: profileInfo.profileName,
    asyncNoesViaMozillaAccount: true,
  };
}
