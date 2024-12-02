/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";
import { zFloorpDesignConfigs } from "../../../apps/common/scripts/global-types/type";
import { zWorkspacesServicesConfigs } from "../../../apps/main/core/common/workspaces/utils/type";
import { zPanelSidebarConfig } from "../../../apps/main/core/common/panel-sidebar/utils/type";

/* Home */
export const zAccountInfo = z.object({
  email: z.string(),
  status: z.string(),
  displayName: z.string(),
  avatarURL: z.string(),
});

export interface HomeData {
  accountName: string | null;
  accountImage: string;
}

export type AccountInfo = z.infer<typeof zAccountInfo>;

/* Tab & Appearance */
export const zDesignFormData = z.object({
  // Global
  design: zFloorpDesignConfigs.shape.globalConfigs.shape.userInterface,
  faviconColor: z.boolean(),

  // Tab Bar
  style: zFloorpDesignConfigs.shape.tabbar.shape.tabbarStyle,
  position: zFloorpDesignConfigs.shape.tabbar.shape.tabbarPosition,

  // Tab
  tabOpenPosition: zFloorpDesignConfigs.shape.tab.shape.tabOpenPosition,
  tabMinHeight: zFloorpDesignConfigs.shape.tab.shape.tabMinHeight,
  tabMinWidth: zFloorpDesignConfigs.shape.tab.shape.tabMinWidth,
  tabPinTitle: zFloorpDesignConfigs.shape.tab.shape.tabPinTitle,
  tabScroll: zFloorpDesignConfigs.shape.tab.shape.tabScroll.shape.enabled,
  tabScrollReverse:
    zFloorpDesignConfigs.shape.tab.shape.tabScroll.shape.reverse,
  tabScrollWrap: zFloorpDesignConfigs.shape.tab.shape.tabScroll.shape.wrap,
  tabDubleClickToClose:
    zFloorpDesignConfigs.shape.tab.shape.tabDubleClickToClose,
});

export type DesignFormData = z.infer<typeof zDesignFormData>;

/* Workspaces */
export const zWorkspacesFormData = z.object({
  enabled: z.boolean(),
  manageOnBms: zWorkspacesServicesConfigs.shape.manageOnBms,
  showWorkspaceNameOnToolbar:
    zWorkspacesServicesConfigs.shape.showWorkspaceNameOnToolbar,
  closePopupAfterClick: zWorkspacesServicesConfigs.shape.closePopupAfterClick,
});

export type WorkspacesFormData = z.infer<typeof zWorkspacesFormData>;

/* About */
export type ConstantsData = {
  MOZ_APP_VERSION: string;
  MOZ_APP_VERSION_DISPLAY: string;
  MOZ_OFFICIAL_BRANDING: boolean;
};

/* Accounts */
export const zAccountsFormData = z.object({
  accountInfo: zAccountInfo,
  profileDir: z.string(),
  profileName: z.string(),
  asyncNoesViaMozillaAccount: z.boolean(),
});

export type AccountsFormData = z.infer<typeof zAccountsFormData>;

/* Panel Sidebar */
export const zPanelSidebarFormData = z.object({
  enabled: z.boolean(),
  autoUnload: zPanelSidebarConfig.shape.autoUnload,
  position_start: zPanelSidebarConfig.shape.position_start,
  displayed: zPanelSidebarConfig.shape.displayed,
  webExtensionRunningEnabled:
    zPanelSidebarConfig.shape.webExtensionRunningEnabled,
});

export type PanelSidebarFormData = z.infer<typeof zPanelSidebarFormData>;
