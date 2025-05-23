import { createEffect, onCleanup } from "solid-js";
import { selectedWorkspaceID } from "./data/data.ts";
import type {
  PanelMultiViewParentElement,
  TWorkspaceID,
} from "./utils/type.ts";
import { zWorkspaceID } from "./utils/type.ts";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./utils/workspaces-static-names.ts";
import { configStore, enabled } from "./data/config.ts";
import type { WorkspaceIcons } from "./utils/workspace-icons.ts";
import type { WorkspacesDataManager } from "./workspacesDataManagerBase.tsx";

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
    this.boundHandleTabClose = this.handleTabClose.bind(this);

    const initWorkspace = () => {
      (globalThis as any).SessionStore.promiseAllWindowsRestored.then(() => {
        this.initializeWorkspace();
        globalThis.addEventListener("TabClose", this.boundHandleTabClose);
      }).catch((error: Error) => {
        console.error("Error waiting for windows restore:", error);
        this.initializeWorkspace();
        globalThis.addEventListener("TabClose", this.boundHandleTabClose);
      });
    };

    initWorkspace();

    createEffect(() => {
      if (!enabled()) {
        return;
      }

      if (selectedWorkspaceID()) {
        this.updateTabsVisibility();
      }
    });

    onCleanup(() => {
      this.cleanup();
    });
  }

  private initializeWorkspace() {
    const maybeSelectedWorkspace = this
      .getMaybeSelectedWorkspacebyVisibleTabs();
    if (maybeSelectedWorkspace) {
      this.changeWorkspace(maybeSelectedWorkspace);
    } else {
      try {
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        this.changeWorkspace(defaultWorkspaceId);
      } catch (e) {
        console.error("Failed to change workspace:", e);
      }
    }
  }

  private boundHandleTabClose: (event: TabEvent) => void;

  public cleanup() {
    globalThis.removeEventListener("TabClose", this.boundHandleTabClose);
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
            globalThis.gBrowser.selectedTab = newTab;
            this.dataManagerCtx.setCurrentWorkspaceID(defaultId);
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
            const newTab = globalThis.gBrowser.addTab("about:newtab", {
              skipAnimation: true,
              triggeringPrincipal: Services.scriptSecurityManager
                .getSystemPrincipal(),
            });
            globalThis.gBrowser.selectedTab = newTab;
          } catch (innerError) {
            console.error("Critical error handling tab close:", innerError);
          }
        }, 0);
      }
    }
  };

  public updateTabsVisibility() {
    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const selectedTab = globalThis.gBrowser.selectedTab;
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
    const tabs = globalThis.gBrowser.tabs;
    for (const tab of tabs) {
      // Set workspaceId if workspaceId is null
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (!workspaceId) {
        this.setWorkspaceIdToAttribute(tab, currentWorkspaceId);
      }

      const chackedWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (chackedWorkspaceId === currentWorkspaceId) {
        globalThis.gBrowser.showTab(tab);
      } else {
        globalThis.gBrowser.hideTab(tab);
      }
    }
  }

  /**
   * Get workspaceId from tab attribute.
   * @param tab The tab.
   * @returns The workspace id.
   */
  getWorkspaceIdFromAttribute(tab: XULElement): TWorkspaceID | null {
    const raw = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
    console.debug("WorkspacesTabManager: raw workspace attribute:", raw);
    if (!raw) {
      console.debug(
        "WorkspacesTabManager: no workspace attribute on tab, returning null",
      );
      return null;
    }
    const clean = raw.replace(/[{}]/g, "");
    console.debug("WorkspacesTabManager: cleaned workspace id:", clean);
    const parseResult = zWorkspaceID.safeParse(clean);
    if (!parseResult.success) {
      console.warn(
        "WorkspacesTabManager: invalid workspace id format:",
        raw,
        parseResult.error,
      );
      return null;
    }
    const wsId = parseResult.data;
    if (!this.dataManagerCtx.isWorkspaceID(wsId)) {
      console.warn(
        "WorkspacesTabManager: workspace id not found in store:",
        wsId,
      );
      return null;
    }
    return wsId;
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
    const tabs = globalThis.gBrowser.tabs;
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
          globalThis.gBrowser.selectedTab = defaultTabs[0];
        } else {
          this.createTabForWorkspace(defaultId, true);
        }

        this.dataManagerCtx.setCurrentWorkspaceID(defaultId);
        this.updateTabsVisibility();
      } else {
        this.createTabForWorkspace(defaultId, true);
      }
    }

    for (let i = tabsToRemove.length - 1; i >= 0; i--) {
      try {
        globalThis.gBrowser.removeTab(tabsToRemove[i]);
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
    const targetURL = url ??
      Services.prefs.getStringPref("browser.startup.homepage");
    const tab = globalThis.gBrowser.addTab(targetURL, {
      skipAnimation: true,
      inBackground: false,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    this.setWorkspaceIdToAttribute(tab, workspaceId);

    if (select) {
      globalThis.gBrowser.selectedTab = tab;
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
        globalThis.gBrowser.selectedTab = willChangeWorkspaceLastShowTab;
      } else {
        const tabToSelect = this.workspaceHasTabs(workspaceId);
        if (tabToSelect) {
          globalThis.gBrowser.selectedTab = tabToSelect;
        } else {
          const nonWorkspaceTab = this.isThereNoWorkspaceTabs();
          if (nonWorkspaceTab !== true) {
            globalThis.gBrowser.selectedTab = nonWorkspaceTab;
            this.setWorkspaceIdToAttribute(nonWorkspaceTab, workspaceId);
          } else {
            this.createTabForWorkspace(workspaceId, true);
          }
        }
      }
      this.dataManagerCtx.setCurrentWorkspaceID(workspaceId);
      this.updateTabsVisibility();
    } catch (e) {
      console.error("Failed to change workspace:", e);

      try {
        const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

        if (defaultId !== workspaceId) {
          this.createTabForWorkspace(defaultId, true);
          this.dataManagerCtx.setCurrentWorkspaceID(defaultId);
          this.updateTabsVisibility();
        } else {
          this.createTabForWorkspace(workspaceId, true);
          this.dataManagerCtx.setCurrentWorkspaceID(workspaceId);
          this.updateTabsVisibility();
        }
      } catch (innerError) {
        console.error("Critical error handling workspace change:", innerError);

        try {
          const newTab = globalThis.gBrowser.addTab("about:newtab", {
            skipAnimation: true,
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
          globalThis.gBrowser.selectedTab = newTab;
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
        globalThis.gBrowser.selectedTab = tab;
      } catch (e) {
        console.error("Failed to create tab for workspace:", e);
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        if (defaultWorkspaceId !== workspaceId) {
          this.changeWorkspace(defaultWorkspaceId);
        }
      }
    } else {
      globalThis.gBrowser.selectedTab = workspaceTabs[0];
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
    for (const tab of globalThis.gBrowser.tabs as XULElement[]) {
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

    if (tab === globalThis.gBrowser.selectedTab && oldWorkspaceId) {
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
    const reopenedTabs = globalThis.TabContextMenu.contextTab.multiselected
      ? globalThis.gBrowser.selectedTabs
      : [globalThis.TabContextMenu.contextTab];

    for (const tab of reopenedTabs) {
      this.moveTabToWorkspace(workspaceId, tab);
      if (tab === globalThis.gBrowser.selectedTab) {
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

  private getMaybeSelectedWorkspacebyVisibleTabs(): TWorkspaceID | null {
    const tabs = (globalThis.gBrowser.visibleTabs as XULElement[]).slice(0, 10);
    const workspaceIdCounts = new Map<TWorkspaceID, number>();

    for (const tab of tabs) {
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (workspaceId) {
        workspaceIdCounts.set(
          workspaceId,
          (workspaceIdCounts.get(workspaceId) || 0) + 1,
        );
      }
    }

    let mostFrequentId: TWorkspaceID | null = null;
    let maxCount = 0;

    workspaceIdCounts.forEach((count, id) => {
      if (count > maxCount) {
        maxCount = count;
        mostFrequentId = id;
      }
    });

    return mostFrequentId;
  }
}
