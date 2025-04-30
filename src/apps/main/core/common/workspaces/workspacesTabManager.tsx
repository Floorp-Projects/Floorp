import { createEffect, onCleanup } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
import type { PanelMultiViewParentElement, TWorkspaceID } from "./utils/type";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./utils/workspaces-static-names";
import { configStore, enabled } from "./data/config";
import type { WorkspaceIcons } from "./utils/workspace-icons";
import type { WorkspacesDataManager } from "./workspacesDataManagerBase";

interface TabEvent extends Event {
  target: XULElement;
  forUnsplit?: boolean;
}

export class WorkspacesTabManager {
  dataManagerCtx: WorkspacesDataManager;
  iconCtx: WorkspaceIcons;
  constructor(iconCtx: WorkspaceIcons, dataManagerCtx: WorkspacesDataManager) {
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;

    createEffect(() => {
      if (workspacesDataStore.selectedID) {
        if (!enabled()) {
          return;
        }

        this.updateTabsVisibility();
      }
    });

    this.boundHandleTabClose = this.handleTabClose.bind(this);

    window.addEventListener("TabClose", this.boundHandleTabClose);

    onCleanup(() => {
      this.cleanup();
    });
  }

  private boundHandleTabClose: (event: TabEvent) => void;

  public cleanup() {
    window.removeEventListener("TabClose", this.boundHandleTabClose);
  }

  private handleTabClose = (event: TabEvent) => {
    const tab = event.target as XULElement;
    const workspaceId = this.getWorkspaceIdFromAttribute(tab);

    if (!workspaceId) return;

    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const isCurrentWorkspace = workspaceId === currentWorkspaceId;
    const workspaceTabs = Array.from(
      document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
      ) as NodeListOf<XULElement>,
    ).filter((t) => t !== tab);

    if (workspaceTabs.length === 0 && isCurrentWorkspace) {
      try {
        const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

        if (defaultId !== workspaceId) {
          const newTab = this.createTabForWorkspace(defaultId, false);

          setTimeout(() => {
            window.gBrowser.selectedTab = newTab;
            setWorkspacesDataStore("selectedID", defaultId);
            this.updateTabsVisibility();
          }, 0);
        } else {
          setTimeout(() => {
            this.createTabForWorkspace(defaultId, true);
          }, 0);
        }
      } catch (e) {
        console.error("Error handling last tab close in workspace:", e);
        setTimeout(() => {
          try {
            const newTab = window.gBrowser.addTab("about:newtab", {
              skipAnimation: true,
              triggeringPrincipal:
                Services.scriptSecurityManager.getSystemPrincipal(),
            });
            window.gBrowser.selectedTab = newTab;
          } catch (innerError) {
            console.error("Critical error handling tab close:", innerError);
          }
        }, 0);
      }
    }
  };

  public updateTabsVisibility() {
    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const selectedTab = window.gBrowser.selectedTab;
    if (
      selectedTab &&
      !selectedTab.hasAttribute(WORKSPACE_LAST_SHOW_ID) &&
      selectedTab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) ===
      currentWorkspaceId
    ) {
      const lastShowWorkspaceTabs = document?.querySelectorAll(
        `[${WORKSPACE_LAST_SHOW_ID}="${currentWorkspaceId}"]`,
      );

      if (lastShowWorkspaceTabs) {
        for (const lastShowWorkspaceTab of lastShowWorkspaceTabs) {
          lastShowWorkspaceTab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
        }
      }

      selectedTab.setAttribute(WORKSPACE_LAST_SHOW_ID, currentWorkspaceId);
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
    const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);

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
    tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, workspaceId);
  }

  /**
   * Remove tab by workspace id.
   * @param workspaceId The workspace id.
   */
  public removeTabByWorkspaceId(workspaceId: TWorkspaceID) {
    const tabs = window.gBrowser.tabs;
    const tabsToRemove = [];

    for (const tab of tabs) {
      if (this.getWorkspaceIdFromAttribute(tab) === workspaceId) {
        tabsToRemove.push(tab);
      }
    }

    if (tabsToRemove.length === 0) return;

    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    if (workspaceId === currentWorkspaceId) {
      const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

      if (defaultId !== workspaceId) {
        const defaultTabs = document?.querySelectorAll(
          `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${defaultId}"]`,
        ) as NodeListOf<XULElement>;

        if (defaultTabs?.length > 0) {
          window.gBrowser.selectedTab = defaultTabs[0];
          setWorkspacesDataStore("selectedID", defaultId);
        } else {
          this.createTabForWorkspace(defaultId, true);
          setWorkspacesDataStore("selectedID", defaultId);
        }

        this.updateTabsVisibility();
      } else {
        this.createTabForWorkspace(defaultId, true);
      }
    }

    for (let i = tabsToRemove.length - 1; i >= 0; i--) {
      try {
        window.gBrowser.removeTab(tabsToRemove[i]);
      } catch (e) {
        console.error("Error removing tab:", e);
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
  createTabForWorkspace(
    workspaceId: TWorkspaceID,
    select = false,
    url?: string,
  ) {
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

    try {
      const willChangeWorkspaceLastShowTab = document?.querySelector(
        `[${WORKSPACE_LAST_SHOW_ID}="${workspaceId}"]`,
      ) as XULElement;

      if (willChangeWorkspaceLastShowTab) {
        window.gBrowser.selectedTab = willChangeWorkspaceLastShowTab;
      } else {
        const tabToSelect = this.workspaceHasTabs(workspaceId);
        if (tabToSelect) {
          window.gBrowser.selectedTab = tabToSelect;
        } else {
          const nonWorkspaceTab = this.isThereNoWorkspaceTabs();
          if (nonWorkspaceTab !== true) {
            window.gBrowser.selectedTab = nonWorkspaceTab;
            this.setWorkspaceIdToAttribute(nonWorkspaceTab, workspaceId);
          } else {
            this.createTabForWorkspace(workspaceId, true);
          }
        }
      }

      setWorkspacesDataStore("selectedID", workspaceId);
      this.updateTabsVisibility();
    } catch (e) {
      console.error("Failed to change workspace:", e);

      try {
        const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

        if (defaultId !== workspaceId) {
          this.createTabForWorkspace(defaultId, true);
          setWorkspacesDataStore("selectedID", defaultId);
          this.updateTabsVisibility();
        } else {
          this.createTabForWorkspace(workspaceId, true);
          setWorkspacesDataStore("selectedID", workspaceId);
          this.updateTabsVisibility();
        }
      } catch (innerError) {
        console.error("Critical error handling workspace change:", innerError);

        try {
          const newTab = window.gBrowser.addTab("about:newtab", {
            skipAnimation: true,
            triggeringPrincipal:
              Services.scriptSecurityManager.getSystemPrincipal(),
          });
          window.gBrowser.selectedTab = newTab;
        } catch (finalError) {
          console.error("Fatal error creating new tab:", finalError);
        }
      }
    }
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
      try {
        const tab = this.createTabForWorkspace(workspaceId);
        this.moveTabToWorkspace(workspaceId, tab);
        window.gBrowser.selectedTab = tab;
      } catch (e) {
        console.error("Failed to create tab for workspace:", e);
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        if (defaultWorkspaceId !== workspaceId) {
          this.changeWorkspace(defaultWorkspaceId);
        }
      }
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
      if (!tab.hasAttribute(WORKSPACE_TAB_ATTRIBUTION_ID)) {
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

    if (tab === window.gBrowser.selectedTab && oldWorkspaceId) {
      const oldWorkspaceTabs = document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${oldWorkspaceId}"]`,
      ) as XULElement[];

      if (oldWorkspaceTabs && oldWorkspaceTabs.length > 0) {
        this.switchToAnotherWorkspaceTab(oldWorkspaceId);
      } else {
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        this.changeWorkspace(defaultWorkspaceId);
      }
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
    this.updateTabsVisibility();
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
