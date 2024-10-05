import type { PrefDatum, PrefDatumWithValue } from "../common/defines.js";

export class NRSettingsChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRSettingsChild created!");
    const window = this.contentWindow;
    if (window.location.port === "5183") {
      console.debug("NRSettingsChild 5183!");
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
      // Cu.exportFunction(this.NRSPrefGet.bind(this), window, {
      //   defineAs: "NRSPrefGet",
      // });
      // Cu.exportFunction(this.NRSPrefSet.bind(this), window, {
      //   defineAs: "NRSPrefSet",
      // });
    }
  }
  NRSPing() {
    return true;
  }
  resolvePrefSet: (() => void) | null = null;
  NRSPrefSet(setting: PrefDatum, callback: () => void) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolvePrefSet = resolve;
    });
    this.sendAsyncMessage("Pref:Set", setting);
    promise.then((_v) => callback());
  }
  resolvePrefGet: ((setting: PrefDatumWithValue) => void) | null = null;
  NRSPrefGet(setting: PrefDatum, callback: (data: PrefDatumWithValue) => void) {
    const promise = new Promise<PrefDatumWithValue>((resolve, _reject) => {
      this.resolvePrefGet = resolve;
    });
    this.sendAsyncMessage("Pref:Get", setting);
    promise.then((v) => callback(v));
  }
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Pref:Set": {
        this.resolvePrefSet?.();
        this.resolvePrefSet = null;
        break;
      }
      case "Pref:Get": {
        this.resolvePrefGet?.(message.data);
        this.resolvePrefGet = null;
        break;
      }
    }
  }
}
