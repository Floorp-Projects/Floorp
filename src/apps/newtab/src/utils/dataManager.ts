import { rpc } from "@/lib/rpc/rpc.ts";

export interface NewTabSettings {
  components: {
    topSites: boolean;
    clock: boolean;
    dashboard: boolean;
  };
  background: {
    type: "none" | "random" | "custom";
    customImage: string | null;
  };
}

const DEFAULT_SETTINGS: NewTabSettings = {
  components: {
    topSites: true,
    clock: true,
    dashboard: true,
  },
  background: {
    type: "none",
    customImage: null,
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
