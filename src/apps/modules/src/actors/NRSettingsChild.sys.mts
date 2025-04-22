import { createBirpc } from "birpc";
import type {
  NRSettingsParentFunctions,
  PrefGetParams,
  PrefSetParams,
} from "../common/defines.ts";

export class NRSettingsChild extends JSWindowActorChild {
  rpc: ReturnType<typeof createBirpc> | null = null;
  constructor() {
    super();
  }
  actorCreated() {
    console.debug("NRSettingsChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.port === "5186" ||
      window?.location.port === "5187" ||
      window?.location.port === "5188" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRSettingsChild 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
      Cu.exportFunction(this.NRSettingsSend.bind(this), window, {
        defineAs: "NRSettingsSend",
      });
      Cu.exportFunction(
        this.NRSettingsRegisterReceiveCallback.bind(this),
        window,
        {
          defineAs: "NRSettingsRegisterReceiveCallback",
        },
      );
    }
  }
  NRSPing() {
    return true;
  }

  sendToPage: ((data: string) => void) | null = null;

  NRSettingsSend(data: string) {
    if (this.sendToPage) {
      this.sendToPage(data);
    }
  }

  NRSettingsRegisterReceiveCallback(callback: (data: string) => void) {
    this.rpc = createBirpc<{}, NRSettingsParentFunctions>(
      {
        getBoolPref: (prefName: string): Promise<boolean | null> => {
          return this.NRSPrefGet({ prefName, prefType: "boolean" });
        },
        getIntPref: (prefName: string): Promise<number | null> => {
          return this.NRSPrefGet({ prefName, prefType: "number" });
        },
        getStringPref: (prefName: string): Promise<string | null> => {
          return this.NRSPrefGet({ prefName, prefType: "string" });
        },
        setBoolPref: (prefName: string, prefValue: boolean): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "boolean" });
        },
        setIntPref: (prefName: string, prefValue: number): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "number" });
        },
        setStringPref: (prefName: string, prefValue: string): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "string" });
        },
      },
      {
        post: (data) => callback(data),
        on: (callback) => {
          this.sendToPage = callback;
        },
        // these are required when using WebSocket
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
  }

  async NRSPrefGet(params: PrefGetParams): Promise<any> {
    try {
      let funcName;
      switch (params.prefType) {
        case "boolean":
          funcName = "getBoolPref";
          break;
        case "number":
          funcName = "getIntPref";
          break;
        case "string":
          funcName = "getStringPref";
          break;
        default:
          throw new Error("Invalid pref type");
      }
      return await this.sendQuery(funcName, {
        name: params.prefName,
      });
    } catch (error) {
      console.error("Error in NRSPrefGet:", error);
      return null;
    }
  }

  async NRSPrefSet(params: PrefSetParams) {
    try {
      let funcName;
      switch (params.prefType) {
        case "boolean":
          funcName = "setBoolPref";
          break;
        case "number":
          funcName = "setIntPref";
          break;
        case "string":
          funcName = "setStringPref";
          break;
        default:
          throw new Error("Invalid pref type");
      }
      return await this.sendQuery(funcName, {
        name: params.prefName,
        prefValue: params.prefValue,
      });
    } catch (error) {
      console.error("Error in NRSPrefSet:", error);
      return null;
    }
  }
}
