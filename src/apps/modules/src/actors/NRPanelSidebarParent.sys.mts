/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type Sidebar = {
  title: string;
  extensionId: string;
  url: string;
  menuId: string;
  keyId: string;
  l10nId: string;
  menuL10nId: string;
  revampL10nId: string;
  iconUrl: string;
  disabled: boolean;
};

type Container = {
  userContextId: number;
  public: boolean;
  icon: string;
  color: number;
  name: string;
  l10nId: string;
};

type MapSidebars = [string, Sidebar][];

export class NRPanelSidebarParent extends JSWindowActorParent {
  constructor() {
    super();
  }

  receiveMessage(message: { name: string; data?: any }) {
    switch (message.name) {
      case "NRPanelSidebar:GetContainerContexts": {
        const containers = this.getContainerContextsData();
        this.sendAsyncMessage(
          "NRPanelSidebar:GetContainerContexts",
          JSON.stringify(containers),
        );
        break;
      }
      case "NRPanelSidebar:GetExtensionPanels": {
        const extensions = this.getExtensionPanelsData();
        this.sendAsyncMessage(
          "NRPanelSidebar:GetExtensionPanels",
          JSON.stringify(extensions),
        );
        break;
      }
    }
  }

  getContainerContextsData() {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );

    function getContainerName(container: Container) {
      if (container.l10nId) {
        return ContextualIdentityService.getUserContextLabel(
          container.userContextId,
        );
      }
      return container.name;
    }

    const containers = ContextualIdentityService.getPublicIdentities();

    const containerOptions = [
      {
        id: "0",
        label: "None",
        icon: "",
      },
      ...containers.map((container: Container) => ({
        id: container.userContextId.toString(),
        label: getContainerName(container),
        icon: "",
      })),
    ];

    return containerOptions;
  }

  getExtensionPanelsData() {
    const getFirefoxSidebarPanels = () => {
      const win = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as Window;
      return Array.from(win.SidebarController.sidebars as MapSidebars)
        .filter((sidebar) => {
          return sidebar[1].extensionId;
        })
        .map((sidebar) => {
          return sidebar[1];
        });
    };

    const extensionPanels = getFirefoxSidebarPanels().map((extension) => ({
      value: extension.extensionId,
      label: extension.title,
      icon: extension.iconUrl,
    }));

    return extensionPanels;
  }
}
