import { NRSettingsParentFunctions } from "../../../../common/settings/rpc.ts";
import { createBirpc } from "birpc";

declare global {
  interface Window {
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

export const rpc = createBirpc<NRSettingsParentFunctions, Record<string, never>>(
  {},
  {
    post: (data) => (globalThis as unknown as Window).NRSettingsSend(data),
    on: (callback) => {
      (globalThis as unknown as Window).NRSettingsRegisterReceiveCallback(callback);
    },
    // these are required when using WebSocket
    serialize: (v) => JSON.stringify(v),
    deserialize: (v) => JSON.parse(v),
  },
);
