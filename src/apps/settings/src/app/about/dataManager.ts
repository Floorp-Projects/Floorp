import type { ConstantsData } from "@/types/pref";

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
