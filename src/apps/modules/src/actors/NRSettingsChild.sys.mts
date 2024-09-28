import type { PrefDatum, PrefDatumWithValue } from "../common/defines.js";

export class NRSettingsChild extends JSWindowActorChild {
  actorCreated() {
    const window = this.contentWindow;
    if (window.location.port === "5183") {
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
      Cu.exportFunction(this.NRSPrefGet.bind(this), window, {
        defineAs: "NRSPrefGet",
      });
      Cu.exportFunction(this.NRSPrefSet.bind(this), window, {
        defineAs: "NRSPrefSet",
      });
    }
  }
  NRSPing() {
    return true;
  }
  resolvePrefSet: () => void = undefined;
  NRSPrefSet(setting: PrefDatum, callback: () => void) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolvePrefSet = resolve;
    });
    this.sendAsyncMessage("Pref:Set", setting);
    promise.then((_v) => callback());
  }
  resolvePrefGet: (setting: PrefDatumWithValue) => void = undefined;
  NRSPrefGet(setting: PrefDatum, callback: (data: PrefDatumWithValue) => void) {
    const promise = new Promise<PrefDatumWithValue>((resolve, _reject) => {
      this.resolvePrefGet = resolve;
    });
    this.sendAsyncMessage("Pref:Get", setting);
    promise.then((v) => callback(v));
  }
  async receiveMessage(message) {
    switch (message.name) {
      case "Pref:Set": {
        this.resolvePrefSet();
        this.resolvePrefSet = undefined;
        break;
      }
      case "Pref:Get": {
        this.resolvePrefGet(message.data);
        this.resolvePrefGet = undefined;
        break;
      }
    }
  }
}
