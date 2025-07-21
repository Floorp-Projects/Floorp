import { rpc } from "../../lib/rpc/rpc.ts";
import type { PanelSidebarFormData } from "@/types/pref.ts";

export async function savePanelSidebarSettings(
  settings: PanelSidebarFormData,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const result = await rpc.getStringPref("floorp.panelSidebar.config");
  if (!result) {
    return null;
  }
  const oldData = JSON.parse(result);

  const newData = {
    ...oldData,
    enabled: settings.enabled,
    autoUnload: settings.autoUnload,
    position_start: settings.position_start,
    globalWidth: settings.globalWidth,
  };

  await Promise.all([
    rpc.setStringPref("floorp.panelSidebar.config", JSON.stringify(newData)),
    rpc.setBoolPref(
      "floorp.panelSidebar.enabled",
      Boolean(settings.enabled ?? true),
    ),
  ]);
}

export async function getPanelSidebarSettings(): Promise<PanelSidebarFormData | null> {
  const [enabled, configResult] = await Promise.all([
    rpc.getBoolPref("floorp.panelSidebar.enabled"),
    rpc.getStringPref("floorp.panelSidebar.config"),
  ]);

  if (!configResult) {
    return null;
  }

  const config = JSON.parse(configResult);
  return {
    enabled,
    autoUnload: config.autoUnload,
    position_start: config.position_start,
    globalWidth: config.globalWidth,
  };
}
