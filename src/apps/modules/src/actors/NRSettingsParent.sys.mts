import { createBirpc } from "birpc";
import type { NRSettingsParentFunctions } from "../common/defines.js";

const parentFunctions:NRSettingsParentFunctions = {
  getBoolPref: function (prefName: string): boolean | null {
    if (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_INVALID) {
      return null;
    }
    return Services.prefs.getBoolPref(prefName)
  },
  getIntPref: function (prefName: string): number | null {
    if (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_INVALID) {
      return null;
    }
    return Services.prefs.getIntPref(prefName)
  },
  getStringPref: function (prefName: string): string | null {
    if (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_INVALID) {
      return null;
    }
    return Services.prefs.getStringPref(prefName)
  },
  setBoolPref: function (prefName: string, prefValue: boolean): void {
    Services.prefs.setBoolPref(prefName,prefValue)
  },
  setIntPref: function (prefName: string, prefValue: number): void {
    Services.prefs.setIntPref(prefName,prefValue)
  },
  setStringPref: function (prefName: string, prefValue: string): void {
    Services.prefs.setStringPref(prefName,prefValue)
  }
}

//TODO: make reject when the prefName is invalid
export class NRSettingsParent extends JSWindowActorParent {
  rpcCallback: Function | null = null;
  rpc;
  constructor() {
    super();
    this.rpc = createBirpc<{},NRSettingsParentFunctions>(
      parentFunctions,
      {
        post: data => this.sendAsyncMessage("birpc",data),
        on: callback => {this.rpcCallback = callback},
        // these are required when using WebSocket
        serialize: v => JSON.stringify(v),
        deserialize: v => JSON.parse(v),
      },
    )
  }
}
