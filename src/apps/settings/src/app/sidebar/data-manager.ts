import type { PanelSidebarFormData } from "@/types";

export async function savePanelSidebarSettings(settings: PanelSidebarFormData) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfigs = await getPanelSidebarSettings();
  const newConfigs = {
    ...oldConfigs,
    ...settings,
  };

  const { enabled, ...configs } = newConfigs;

  return await Promise.all([
    window.NRSPrefSet({
      prefName: "floorp.panelSidebar.config",
      prefValue: JSON.stringify(configs),
      prefType: "string",
    }),
    window.NRSPrefSet({
      prefName: "floorp.panelSidebar.enabled",
      prefValue: enabled ?? true,
      prefType: "boolean",
    }),
  ]);
}

export async function getPanelSidebarSettings(): Promise<PanelSidebarFormData> {
  const [enabled, configs] = await Promise.all([
    getPanelSidebarEnabled(),
    getPanelSidebarConfigsExcludeEnabled(),
  ]);

  return {
    ...configs,
    enabled,
  };
}

async function getPanelSidebarEnabled(): Promise<boolean> {
  const data = await window.NRSPrefGet({
    prefName: "floorp.panelSidebar.enabled",
    prefType: "boolean",
  });
  return JSON.parse(data).prefValue;
}

async function getPanelSidebarConfigsExcludeEnabled(): Promise<Omit<PanelSidebarFormData, "enabled">> {
  const data = await window.NRSPrefGet({
    prefName: "floorp.panelSidebar.config",
    prefType: "string",
  });
  const parsedData = JSON.parse(data);
  return JSON.parse(parsedData.prefValue);
}