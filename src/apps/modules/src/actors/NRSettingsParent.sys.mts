import type { PrefDatum, PrefDatumWithValue } from "../common/defines.js";

export class NRSettingsParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    console.log(message);
    switch (message.name) {
      case "Pref:Set": {
        const d = message.data as PrefDatumWithValue;
        switch (d.prefType) {
          case "boolean": {
            Services.prefs.setBoolPref(d.prefName, d.prefValue);
            this.sendAsyncMessage("Pref:Set");
            break;
          }
          case "integer": {
            Services.prefs.setIntPref(d.prefName, d.prefValue);
            this.sendAsyncMessage("Pref:Set");
            break;
          }
          case "string": {
            Services.prefs.setStringPref(d.prefName, d.prefValue);
            this.sendAsyncMessage("Pref:Set");
            break;
          }
        }
        break;
      }
      case "Pref:Get": {
        const d = message.data as PrefDatum;
        switch (d.prefType) {
          case "boolean": {
            const prefValue = Services.prefs.getBoolPref(d.prefName);
            this.sendAsyncMessage("Pref:Get", {
              prefName: d.prefName,
              prefType: d.prefType,
              prefValue,
            } as PrefDatumWithValue);
            break;
          }
          case "integer": {
            const prefValue = Services.prefs.getIntPref(d.prefName);
            this.sendAsyncMessage("Pref:Get", {
              prefName: d.prefName,
              prefType: d.prefType,
              prefValue,
            } as PrefDatumWithValue);
            break;
          }
          case "string": {
            const prefValue = Services.prefs.getStringPref(d.prefName);
            this.sendAsyncMessage("Pref:Get", {
              prefName: d.prefName,
              prefType: d.prefType,
              prefValue,
            } as PrefDatumWithValue);
            break;
          }
        }
        break;
      }
    }
  }
}
