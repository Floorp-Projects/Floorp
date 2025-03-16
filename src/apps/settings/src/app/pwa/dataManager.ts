import { rpc } from "../../lib/rpc/rpc.ts";
import {
  TProgressiveWebAppFormData,
  TProgressiveWebAppObject,
} from "@/types/pref.ts";

export async function savePwaSettings(
  settings: TProgressiveWebAppFormData,
): Promise<void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const { enabled, ...configs } = settings;

  await Promise.all([
    rpc.setStringPref("floorp.browser.ssb.config", JSON.stringify(configs)),
    rpc.setBoolPref("floorp.browser.ssb.enabled", enabled),
  ]);
}

export async function getPwaSettings(): Promise<TProgressiveWebAppFormData> {
  const [enabled, configStr] = await Promise.all([
    rpc.getBoolPref("floorp.browser.ssb.enabled"),
    rpc.getStringPref("floorp.browser.ssb.config"),
  ]);

  const configs = configStr ? JSON.parse(configStr) : {};

  return {
    enabled,
    ...configs,
  };
}

export async function getInstalledApps(): Promise<TProgressiveWebAppObject> {
  return new Promise((resolve, _reject) => {
    try {
      window.NRGetInstalledApps((installedApps: string) => {
        if (!installedApps) {
          resolve({});
          return;
        }
        try {
          const parsed = JSON.parse(installedApps);
          resolve(parsed);
        } catch (_e) {
          console.error("Failed to parse installed apps:", installedApps);
          resolve({});
        }
      });
    } catch (e) {
      console.error("Failed to get installed apps:", e);
      resolve({});
    }
  });
}

export async function renamePwaApp(id: string, newName: string): Promise<void> {
  return new Promise((resolve, reject) => {
    try {
      window.NRRenameSsb(id, newName);
      resolve();
    } catch (e) {
      console.error("Failed to rename PWA:", e);
      reject(e);
    }
  });
}

export async function uninstallPwaApp(id: string): Promise<void> {
  return new Promise((resolve, reject) => {
    try {
      window.NRUninstallSsb(id);
      resolve();
    } catch (e) {
      console.error("Failed to uninstall PWA:", e);
      reject(e);
    }
  });
}
