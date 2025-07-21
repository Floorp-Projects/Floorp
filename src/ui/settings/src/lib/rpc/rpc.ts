import { NRSettingsParentFunctions } from "../../../../common/settings/rpc.ts";
import { createBirpc } from "birpc";

declare const Services: any;
declare global {
  interface Window {
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

const isLocalhost5183 = import.meta.url?.includes("localhost:5183");

const directServicesFunctions: NRSettingsParentFunctions = {
  getBoolPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_BOOL) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getBoolPref(prefName));
  },
  getIntPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_INT) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getIntPref(prefName));
  },
  getStringPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_STRING) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getStringPref(prefName));
  },
  setBoolPref: (prefName, value) => {
    Services.prefs.setBoolPref(prefName, value);
    return Promise.resolve();
  },
  setIntPref: (prefName, value) => {
    Services.prefs.setIntPref(prefName, value);
    return Promise.resolve();
  },
  setStringPref: (prefName, value) => {
    Services.prefs.setStringPref(prefName, value);
    return Promise.resolve();
  },
};

export const rpc = isLocalhost5183
  ? createBirpc<NRSettingsParentFunctions, Record<string, never>>(
      {},
      {
        post: (data) => (globalThis as unknown as Window).NRSettingsSend(data),
        on: (callback) => {
          (globalThis as unknown as Window).NRSettingsRegisterReceiveCallback(
            callback,
          );
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    )
  : directServicesFunctions;
