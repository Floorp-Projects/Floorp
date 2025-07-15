import { rpc } from "@/lib/rpc/rpc.ts";

declare global {
  interface Window {
    NRGetFolderPathFromDialog: (callback: (data: string) => void) => void;
    NRGetRandomImageFromFolder: (
      folderPath: string,
      callback: (data: string) => void,
    ) => void;
    NRGetAllImagesFromFolder: (
      folderPath: string,
      callback: (data: string) => void,
    ) => void;
  }
}

export interface NewTabSettings {
  components: {
    topSites: boolean;
    clock: boolean;
    searchBar?: boolean;
  };
  background: {
    type: "none" | "random" | "custom" | "folderPath" | "floorp";
    customImage: string | null;
    fileName: string | null;
    folderPath?: string | null;
    selectedFloorp?: string | null;
    slideshow: {
      enabled: boolean;
      interval: number; // seconds
    };
  };
  searchBar: {
    searchEngine: string;
  };
}

export interface FolderPathResult {
  path: string | null;
  success: boolean;
}

export interface RandomImageResult {
  image: string | null;
  fileName: string | null;
  success: boolean;
}

export interface AllImagesResult {
  images: string[];
  success: boolean;
}

const DEFAULT_SETTINGS: NewTabSettings = {
  components: {
    topSites: true,
    clock: true,
    searchBar: true,
  },
  background: {
    type: "random",
    customImage: null,
    fileName: null,
    folderPath: null,
    selectedFloorp: null,
    slideshow: {
      enabled: false,
      interval: 30, // 30 seconds default
    },
  },
  searchBar: {
    searchEngine: "default",
  },
};

let savePromise: Promise<void> | null = null;

export async function saveNewTabSettings(
  settings: NewTabSettings,
): Promise<void> {
  try {
    if (savePromise) {
      await savePromise;
    }

    savePromise = (async () => {
      const current = await getNewTabSettings();

      const newSettings = {
        ...current,
        ...settings,
      };

      await rpc.setStringPref(
        "floorp.newtab.configs",
        JSON.stringify(newSettings),
      );
    })();

    await savePromise;
  } catch (e) {
    console.error("Failed to save newtab settings:", e);
    throw e;
  } finally {
    savePromise = null;
  }
}

export async function getNewTabSettings(): Promise<NewTabSettings> {
  try {
    const result = await rpc.getStringPref("floorp.newtab.configs");
    if (!result) {
      return DEFAULT_SETTINGS;
    }

    const settings = JSON.parse(result);
    return {
      ...DEFAULT_SETTINGS,
      ...settings,
    };
  } catch (e) {
    console.error("Failed to load newtab settings:", e);
    return DEFAULT_SETTINGS;
  }
}

export async function getFolderPathFromDialog(): Promise<FolderPathResult> {
  return await new Promise((resolve) => {
    window.NRGetFolderPathFromDialog((data: string) => {
      const result = JSON.parse(data);
      resolve(result);
    });
  });
}

export async function getRandomImageFromFolder(
  folderPath: string,
): Promise<RandomImageResult> {
  return await new Promise((resolve) => {
    window.NRGetRandomImageFromFolder(folderPath, (data: string) => {
      const result = JSON.parse(data);
      resolve(result);
    });
  });
}

export async function getAllImagesFromFolder(
  folderPath: string,
): Promise<AllImagesResult> {
  return await new Promise((resolve) => {
    window.NRGetAllImagesFromFolder(folderPath, (data: string) => {
      const result = JSON.parse(data);
      resolve(result);
    });
  });
}
