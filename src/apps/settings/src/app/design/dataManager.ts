import { rpc } from "@/lib/rpc/rpc.ts";
import type { DesignFormData } from "@/types/pref.ts";

export async function saveDesignSettings(
  settings: DesignFormData,
): Promise<null | void> {
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
      faviconColor: settings.faviconColor,
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
    uiCustomization: {
      ...oldData.uiCustomization,
      navbar: {
        position: settings.navbarPosition,
        searchBarTop: settings.searchBarTop,
      },
      display: {
        disableFullscreenNotification: settings.disableFullscreenNotification,
        deleteBrowserBorder: settings.deleteBrowserBorder,
        hideUnifiedExtensionsButton: settings.hideUnifiedExtensionsButton,
      },
      special: {
        optimizeForTreeStyleTab: settings.optimizeForTreeStyleTab,
        hideForwardBackwardButton: settings.hideForwardBackwardButton,
        stgLikeWorkspaces: settings.stgLikeWorkspaces,
      },
      multirowTab: {
        newtabInsideEnabled: settings.multirowTabNewtabInside,
      },
    },
  };
  rpc.setStringPref("floorp.design.configs", JSON.stringify(newData));
}

export async function getDesignSettings(): Promise<DesignFormData | null> {
  const result = await rpc.getStringPref("floorp.design.configs");
  if (!result) {
    return null;
  }
  const data = JSON.parse(result);
  const formData: DesignFormData = {
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
    faviconColor: data.globalConfigs.faviconColor,

    navbarPosition: data.uiCustomization.navbar.position,
    searchBarTop: data.uiCustomization.navbar.searchBarTop,
    disableFullscreenNotification:
      data.uiCustomization.display.disableFullscreenNotification,
    deleteBrowserBorder: data.uiCustomization.display.deleteBrowserBorder,
    hideUnifiedExtensionsButton:
      data.uiCustomization.display.hideUnifiedExtensionsButton,
    optimizeForTreeStyleTab:
      data.uiCustomization.special.optimizeForTreeStyleTab,
    hideForwardBackwardButton:
      data.uiCustomization.special.hideForwardBackwardButton,
    stgLikeWorkspaces: data.uiCustomization.special.stgLikeWorkspaces,
    multirowTabNewtabInside:
      data.uiCustomization.multirowTab.newtabInsideEnabled,
  };
  return formData;
}
