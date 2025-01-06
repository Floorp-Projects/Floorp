import { createEffect } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
import { PanelMultiViewParentElement, TWorkspaceID } from "./utils/type";
import { WORKSPACE_LAST_SHOW_ID, WORKSPACE_TAB_ATTRIBUTION_ID } from "./utils/workspaces-static-names";
import { configStore } from "./data/config";
import { WorkspaceIcons } from "./utils/workspace-icons";
import { WorkspacesDataManager } from "./workspacesDataManagerBase";

export class WorkspacesTabManager {
  dataManagerCtx: WorkspacesDataManager;
  iconCtx: WorkspaceIcons
  constructor(iconCtx:WorkspaceIcons, dataManagerCtx: WorkspacesDataManager) {
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    createEffect(() => {
      if (workspacesDataStore.selectedID) {
        this.updateTabsVisibility()
      }
    })
  }
  /**
   * Check Tabs visibility.
   * @returns void
   */
  public updateTabsVisibility() {
    // Get Current Workspace & Workspace Id
    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    // Last Show Workspace Attribute
    const selectedTab = window.gBrowser.selectedTab;
    if (
      selectedTab &&
      !selectedTab.hasAttribute(
        WORKSPACE_LAST_SHOW_ID,
      ) &&
      selectedTab.getAttribute(
        WORKSPACE_TAB_ATTRIBUTION_ID
      ) === currentWorkspaceId
    ) {
      const lastShowWorkspaceTabs = document?.querySelectorAll(
        `[${WORKSPACE_LAST_SHOW_ID}="${currentWorkspaceId}"]`,
      );

      if (lastShowWorkspaceTabs) {
        for (const lastShowWorkspaceTab of lastShowWorkspaceTabs) {
          lastShowWorkspaceTab.removeAttribute(
            WORKSPACE_LAST_SHOW_ID,
          );
        }
      }

      selectedTab.setAttribute(
        WORKSPACE_LAST_SHOW_ID,
        currentWorkspaceId,
      );
    }

    // Check Tabs visibility
    const tabs = window.gBrowser.tabs;
    for (const tab of tabs) {
      // Set workspaceId if workspaceId is null
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (!workspaceId) {
        this.setWorkspaceIdToAttribute(tab, currentWorkspaceId);
      }

      const chackedWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (chackedWorkspaceId === currentWorkspaceId) {
        window.gBrowser.showTab(tab);
      } else {
        window.gBrowser.hideTab(tab);
      }
    }
  }

  /**
   * Get workspaceId from tab attribute.
   * @param tab The tab.
   * @returns The workspace id.
   */
  getWorkspaceIdFromAttribute(tab: XULElement): TWorkspaceID | null {
    const workspaceId = tab.getAttribute(
      WORKSPACE_TAB_ATTRIBUTION_ID,
    );

    if (workspaceId && this.dataManagerCtx.isWorkspaceID(workspaceId)) {
      return workspaceId;
    } else {
      return null;
    }
  }

  /**
   * Set workspaceId to tab attribute.
   * @param tab The tab.
   * @param workspaceId The workspace id.
   */
  setWorkspaceIdToAttribute(tab: XULElement, workspaceId: TWorkspaceID) {
    tab.setAttribute(
      WORKSPACE_TAB_ATTRIBUTION_ID,
      workspaceId,
    );
  }

  /**
   * Remove tab by workspace id.
   * @param workspaceId The workspace id.
   */
  public removeTabByWorkspaceId(workspaceId: TWorkspaceID) {
    const tabs = window.gBrowser.tabs;
    for (const tab of tabs) {
      if (this.getWorkspaceIdFromAttribute(tab) === workspaceId) {
        window.gBrowser.removeTab(tab);
      }
    }
  }

  /**
   * Create tab for workspace.
   * @param workspaceId The workspace id.
   * @param url The url will be opened in the tab.
   * @param select will select tab if true.
   * @returns The created tab.
   */
  createTabForWorkspace(workspaceId: TWorkspaceID, select = false, url?: string) {
    const targetURL =
      url ?? Services.prefs.getStringPref("browser.startup.homepage");
    const tab = window.gBrowser.addTab(targetURL, {
      skipAnimation: true,
      inBackground: false,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    this.setWorkspaceIdToAttribute(tab, workspaceId);

    if (select) {
      window.gBrowser.selectedTab = tab;
    }
    return tab;
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspace(workspaceId: TWorkspaceID) {
    if (
      configStore.closePopupAfterClick &&
      this.targetToolbarItem?.hasAttribute("open")
    ) {
      this.panelUITargetElement?.hidePopup();
    }

    const willChangeWorkspaceLastShowTab = document?.querySelector(
      `[${WORKSPACE_LAST_SHOW_ID}="${workspaceId}"]`,
    );

    if (willChangeWorkspaceLastShowTab) {
      window.gBrowser.selectedTab = willChangeWorkspaceLastShowTab;
    } else if (this.workspaceHasTabs(workspaceId)) {
      window.gBrowser.selectedTab = this.workspaceHasTabs(workspaceId);
    } else if (this.isThereNoWorkspaceTabs() !== true) {
      window.gBrowser.selectedTab = this.isThereNoWorkspaceTabs();
    } else {
      this.createTabForWorkspace(workspaceId, true);
    }

    setWorkspacesDataStore("selectedID",workspaceId)
    this.changeWorkspaceToolbarState(workspaceId);
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspaceToolbarState(workspaceId: TWorkspaceID) {
    const gWorkspaceIcons = this.iconCtx;
    const targetToolbarItem = document?.querySelector(
      "#workspaces-toolbar-button",
    ) as XULElement;

    const workspace = this.dataManagerCtx.getRawWorkspace(workspaceId);

    if (configStore.showWorkspaceNameOnToolbar) {
      targetToolbarItem?.setAttribute("label", workspace.name);
    } else {
      targetToolbarItem?.removeAttribute("label");
    }

    (document?.documentElement as XULElement).style.setProperty(
      "--workspaces-toolbar-button-icon",
      `url(${gWorkspaceIcons.getWorkspaceIconUrl(workspace.icon)})`,
    );
  }


  /**
   * Switch to another workspace tab.
   * @param workspaceId The workspace id.
   * @returns void
   */
  switchToAnotherWorkspaceTab(workspaceId: TWorkspaceID) {
    const workspaceTabs = document?.querySelectorAll(
      `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
    ) as XULElement[];

    if (!workspaceTabs?.length) {
      const tab = this.createTabForWorkspace(workspaceId);
      this.moveTabToWorkspace(workspaceId, tab);
      window.gBrowser.selectedTab = tab;
    } else {
      window.gBrowser.selectedTab = workspaceTabs[0];
    }
  }

  /**
   * Check if workspace has tabs.
   * @param workspaceId The workspace id.
   * @returns true if workspace has tabs.
   */
  public workspaceHasTabs(workspaceId: string) {
    const workspaceTabs = document?.querySelectorAll(
      `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
    ) as XULElement[];
    return workspaceTabs?.length > 0 ? workspaceTabs[0] : false;
  }

  /**
   * Check if there is no workspace tabs.
   * @returns true if there is no workspace tabs if false, return tab.
   */
  public isThereNoWorkspaceTabs() {
    for (const tab of window.gBrowser.tabs as XULElement[]) {
      if (
        !tab.hasAttribute(
          WORKSPACE_TAB_ATTRIBUTION_ID,
        )
      ) {
        return tab;
      }
    }
    return true;
  }

  /**
   * Move tabs to workspace.
   * @param workspaceId The workspace id.
   */
  async moveTabToWorkspace(workspaceId: TWorkspaceID, tab: XULElement) {
    const oldWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
    this.setWorkspaceIdToAttribute(tab, workspaceId);
    if (tab === window.gBrowser.selectedTab) {
      this.switchToAnotherWorkspaceTab(
        oldWorkspaceId ?? workspacesDataStore.defaultID,
      );
    }
  }

  /**
   * Move tabs to workspace from tab context menu.
   * @param workspaceId The workspace id.
   */
  public moveTabsToWorkspaceFromTabContextMenu(workspaceId: TWorkspaceID) {
    const reopenedTabs = window.TabContextMenu.contextTab.multiselected
      ? window.gBrowser.selectedTabs
      : [window.TabContextMenu.contextTab];

    for (const tab of reopenedTabs) {
      this.moveTabToWorkspace(workspaceId, tab);
      if (tab === window.gBrowser.selectedTab) {
        this.switchToAnotherWorkspaceTab(workspaceId);
      }
    }
  }

  /**
   * Returns target toolbar item.
   * @returns The target toolbar item.
   */
  private get targetToolbarItem(): XULElement | undefined | null {
    return document?.querySelector("#workspaces-toolbar-button");
  }

  /**
   * Returns panel UI target element.
   * @returns The panel UI target element.
   */
  private get panelUITargetElement():
    | PanelMultiViewParentElement
    | undefined
    | null {
    return document?.querySelector("#customizationui-widget-panel");
  }
}