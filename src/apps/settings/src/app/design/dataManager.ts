import { NRSettingsParentFunctions } from "../../../../common/settings/rpc.ts";
import { createBirpc } from "birpc";

const rpc = createBirpc<NRSettingsParentFunctions, {}>(
  {},
  {
    post: (data) => window.NRSettingsSend(data),
    on: (callback) => {
      window.NRSettingsRegisterReceiveCallback(callback);
    },
    // these are required when using WebSocket
    serialize: (v) => JSON.stringify(v),
    deserialize: (v) => JSON.parse(v),
  },
);

export async function saveDesignSettings(settings: any) {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const result = await rpc.getStringPref("floorp.design.configs");
  if (!result) {
    return null;
  }
  const oldData = JSON.parse(result);

  const newData = {
    ...oldData,
    globalConfigs: {
      ...oldData.globalConfigs,
      userInterface: settings.design,
    },
    tabbar: {
      ...oldData.tabbar,
      tabbarPosition: settings.position,
      tabbarStyle: settings.style,
    },
    tab: {
      ...oldData.tab,
      tabScroll: {
        ...oldData.tab.tabScroll,
        reverse: settings.tabScrollReverse,
        wrap: settings.tabScrollWrap,
        enabled: settings.tabScroll,
      },
      tabOpenPosition: settings.tabOpenPosition,
      tabMinHeight: settings.tabMinHeight,
      tabMinWidth: settings.tabMinWidth,
      tabPinTitle: settings.tabPinTitle,
      tabDubleClickToClose: settings.tabDubleClickToClose,
    },
  };
  rpc.setStringPref("floorp.design.configs", JSON.stringify(newData));
}

export async function getDesignSettings(): Promise<DesignSettings> {
  const result = await rpc.getStringPref("floorp.design.configs");
  if (!result) {
    return null;
  }
  const data = JSON.parse(result);
  const formData: DesignSettings = {
    design: data.globalConfigs.userInterface,
    position: data.tabbar.tabbarPosition,
    style: data.tabbar.tabbarStyle,
    tabOpenPosition: data.tab.tabOpenPosition,
    tabMinHeight: data.tab.tabMinHeight,
    tabMinWidth: data.tab.tabMinWidth,
    tabPinTitle: data.tab.tabPinTitle,
    tabScrollReverse: data.tab.tabScroll.reverse,
    tabScrollWrap: data.tab.tabScroll.wrap,
    tabDubleClickToClose: data.tab.tabDubleClickToClose,
    tabScroll: data.tab.tabScroll.enabled,
  };
  return formData;
}
