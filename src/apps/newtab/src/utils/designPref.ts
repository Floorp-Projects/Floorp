import { rpc } from "@/lib/rpc/rpc.ts";

// Helper to get current disableFloorpStart flag from floorp.design.configs
export async function getDisableFloorpStart(): Promise<boolean> {
  try {
    const result = await rpc.getStringPref("floorp.design.configs");
    if (!result) return false;
    const data = JSON.parse(result);
    return !!data?.uiCustomization?.disableFloorpStart;
  } catch (e) {
    console.error("Failed to get disableFloorpStart:", e);
    return false;
  }
}

// Update only the disableFloorpStart flag while preserving other configs
export async function setDisableFloorpStart(disabled: boolean): Promise<void> {
  try {
    const result = await rpc.getStringPref("floorp.design.configs");
    if (!result) return; // can't update if base config missing
    const data = JSON.parse(result);
    if (!data.uiCustomization) data.uiCustomization = {};
    data.uiCustomization.disableFloorpStart = disabled;
    await rpc.setStringPref("floorp.design.configs", JSON.stringify(data));
  } catch (e) {
    console.error("Failed to set disableFloorpStart:", e);
  }
}
