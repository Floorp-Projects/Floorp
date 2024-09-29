/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SplitViewStaticNames } from "./utils/static-names";
import { render } from "@nora/solid-xul";
import type { SplitViewData, Tab, TabEvent } from "./utils/type";
import splitViewStyles from "./style.css?inline";
import {
  currentSplitView,
  setCurrentSplitView,
  setSplitViewData,
  splitViewData,
  fixedSplitViewData,
  setFixedSplitViewData,
} from "./utils/data";
import { FixedTabUtils } from "./fixedTabUtils";

export class SplitView {
  private static instance: SplitView;
  static getInstance() {
    if (!SplitView.instance) {
      SplitView.instance = new SplitView();
    }
    return SplitView.instance;
  }

  /**
   * Initializes the SplitView instance.
   * @description: Initializes the SplitView instance.
   * This constructor sets up the necessary event listeners and initializes the split view functionality.
   * It renders the style element, sets initial preferences, and adds various event listeners and progress listeners.
   */
  constructor() {
    render(this.StyleElement, document?.head, {
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      hotCtx: (import.meta as any).hot,
    });

    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
    window.addEventListener("TabClose", (event: TabEvent) =>
      this.handleTabClose(event),
    );
    window.gBrowser.addProgressListener(this.listener);
    this.addMainPopupShowingListener();
    this.checkAllTabHaveSplitViewAttribute();
    window.gBrowser.addProgressListener(this.listener);
  }

  /**
   * @description: Progress listener. If the selected tab changes, update the split view for creating a new view.
   */
  private listener = {
    onLocationChange: () => {
      this.checkAllTabHaveSplitViewAttribute();
      const tab = window.gBrowser.selectedTab as Tab;
      if (tab) {
        this.updateSplitView(tab);
        tab.linkedBrowser.docShellIsActive = true;
        tab.linkedBrowser.renderLayers = true;
        document
          ?.getElementById(tab.linkedPanel)
          ?.classList.add("deck-selected");
      }
    },
  };

  /**
   * @description: Style element for the split view.
   * @returns {Element} The style element.
   */
  private StyleElement(): Element {
    return (<style>{splitViewStyles}</style>) as Element;
  }

  /**
   * @description: Get the tab browser panel element.
   * @returns {XULElement} The tab browser panel element.
   */
  get tabBrowserPanel(): XULElement {
    return document?.getElementById("tabbrowser-tabpanels") as XULElement;
  }

  /**
   * @description: Get the generated UUID.
   * @returns {string} The generated UUID.
   */
  private get getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  /**
   * @description: Get the tab ID.
   * @param {Tab} tab - The tab element.
   * @returns {string} The tab ID.
   */
  private getTabId(tab: Tab): string {
    if (!tab) {
      throw new Error("Tab is not defined");
    }
    return tab.getAttribute(SplitViewStaticNames.TabAttributionId) ?? "";
  }

  /**
   * @description: Set the tab ID.
   * @param {Tab} tab - The tab element.
   * @param {string} id - The tab ID.
   * @returns {string} The tab ID.
   */
  private setTabId(tab: Tab, id: string): string {
    const currentId = tab.getAttribute(SplitViewStaticNames.TabAttributionId);
    if (currentId) {
      return currentId;
    }

    tab.setAttribute(SplitViewStaticNames.TabAttributionId, id);
    return id;
  }

  /**
   * @description: Get the tab element by the tab ID.
   * @param {string} id - The tab ID.
   * @returns {Tab} The tab element.
   */
  public getTabById(id: string): Tab {
    return document?.querySelector(
      `[${SplitViewStaticNames.TabAttributionId}="${id}"]`,
    ) as Tab;
  }

  /**
   * @description: Check if all tabs have the split view attribute.
   */
  private checkAllTabHaveSplitViewAttribute() {
    for (const tab of window.gBrowser.tabs) {
      if (!tab.getAttribute(SplitViewStaticNames.TabAttributionId)) {
        this.setTabId(tab, this.getGeneratedUuid);
      }
    }
  }

  /**
   * @description: Handle the tab close event.
   * @param {TabEvent} event - The tab event.
   */
  private handleTabClose(event: TabEvent) {
    const tab = event.target;
    const groupIndex = splitViewData().findIndex((group) =>
      group.tabIds.includes(this.getTabId(tab)),
    );

    if (groupIndex < 0) {
      return;
    }

    this.removeTabFromGroup(tab, groupIndex, event.forUnsplit);
  }

  /**
   * @description: Remove the tab from the group.
   * @param {Tab} tab - The tab element.
   * @param {number} groupIndex - The group index.
   * @param {boolean} forUnsplit - Whether to unsplit the tab.
   */
  private removeTabFromGroup(
    tab: Tab,
    groupIndex: number,
    forUnsplit: boolean,
  ) {
    const group = splitViewData()[groupIndex];
    const tabIndex = group.tabIds.indexOf(this.getTabId(tab));
    group.tabIds.splice(tabIndex, 1);

    this.resetTabState(tab, forUnsplit);

    if (group.tabIds.length < 2) {
      this.removeGroup(groupIndex);
    } else {
      this.updateSplitView(
        this.getTabById(group.tabIds[group.tabIds.length - 1]),
      );
    }
  }

  /**
   * @description: Reset the tab state.
   * @param {Tab} tab - The tab element.
   * @param {boolean} forUnsplit - Whether to unsplit the tab.
   */
  private resetTabState(tab: Tab, forUnsplit: boolean) {
    tab.splitView = false;
    tab.linkedBrowser.spliting = false;

    const container = tab.linkedBrowser.closest(
      ".browserSidebarContainer",
    ) as XULElement;

    if (container) {
      container.removeAttribute("split");
      container.removeAttribute("style");

      if (!forUnsplit) {
        tab.linkedBrowser.docShellIsActive = false;
        container.style.display = "none";
      } else {
        container.style.flex = "";
        container.style.order = "";
      }
    }
  }

  /**
   * @description: Remove the group.
   * @param {number} groupIndex - The group index.
   */
  private removeGroup(groupIndex: number) {
    if (currentSplitView() === groupIndex) {
      this.resetSplitView();
    }
    setSplitViewData((prev) => prev.splice(groupIndex, 1));
  }

  private resetSplitView() {
    for (const tab of splitViewData()[currentSplitView()].tabIds) {
      this.resetTabState(this.getTabById(tab), true);
    }

    setCurrentSplitView(-1);

    this.tabBrowserPanel.removeAttribute("split-view");
    this.tabBrowserPanel.style.display = "";
    this.tabBrowserPanel.style.flexDirection = "";

    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
  }

  /**
   * @description: Handle the split view panel reverse option click.
   * @param {boolean} reverse - Whether to reverse the split view.
   */
  public handleSplitViewPanelRevseOptionClick(reverse: boolean) {
    this.updateSplitView(window.gBrowser.selectedTab, reverse === true, null);
  }

  /**
   * @description: Handle the split view panel type option click.
   * @param {string} method - The split view method.
   */
  public handleSplitViewPanelTypeOptionClick(method: "row" | "column") {
    this.updateSplitView(window.gBrowser.selectedTab, null, method);
  }

  /**
   * @description: Add the main popup showing listener.
   */
  private addMainPopupShowingListener() {
    const mainPopup = document?.getElementById("mainPopupSet");
    mainPopup?.addEventListener("popupshowing", () => {
      if (!window.TabContextMenu.contextTab) {
        return;
      }

      const elem = document?.getElementById("context_splittabs") as XULElement;
      if (elem) {
        elem.setAttribute("disabled", String(this.contextMenuShouldBeDisabled));
        if (elem.getAttribute("disabled") === "true") {
          document?.l10n?.setAttributes(
            elem,
            "floorp-split-view-open-menu-disabled",
          );
        } else {
          document?.l10n?.setAttributes(elem, "floorp-split-view-open-menu");
        }
      }
    });
  }

  /**
   * @description: Check if the context menu should be disabled.
   * @returns {boolean} Whether the context menu should be disabled.
   */
  private get contextMenuShouldBeDisabled() {
    const excludeFixedTabDataList = splitViewData().filter(
      (group) => group.fixedMode !== true,
    );

    return (
      window.TabContextMenu.contextTab === window.gBrowser.selectedTab ||
      excludeFixedTabDataList.some((group) =>
        group.tabIds.includes(this.getTabId(window.TabContextMenu.contextTab)),
      ) ||
      excludeFixedTabDataList.some((group) =>
        group.tabIds.includes(this.getTabId(window.gBrowser.selectedTab)),
      )
    );
  }

  /**
   * @description: Split the tabs. it should be called from the context menu.
   */
  public contextSplitTabs() {
    if (window.TabContextMenu.contextTab === window.gBrowser.selectedTab) {
      return;
    }
    const tab = [window.TabContextMenu.contextTab, window.gBrowser.selectedTab];
    this.splitTabs(tab);
  }

  /**
   * @description: If a tab does not have a SplitView, This selected tab will be used as a fixed tab.
   */
  public splitContextFixedTab() {
    if (window.TabContextMenu.contextTab === window.gBrowser.selectedTab) {
      return;
    }
    this.splitFixedTab(window.TabContextMenu.contextTab);
  }

  /**
   * @description: Split the tabs from the current tab.
   */
  public splitFromCsk() {
    const tab = [this.getAnotherTab(), window.gBrowser.selectedTab];
    this.splitTabs(tab);
  }

  private getAnotherTab() {
    let result =
      window.gBrowser.tabs[
        window.gBrowser.tabs.indexOf(window.gBrowser.selectedTab) - 1
      ];
    if (!result) {
      result =
        window.gBrowser.tabs[
          window.gBrowser.tabs.indexOf(window.gBrowser.selectedTab) + 1
        ];
    }
    return result ?? null;
  }

  /**
   * @description: Split the tabs.
   * @param {Tab[]} tabs - The tab elements.
   */
  private splitTabs(tabs: Tab[]) {
    if (tabs.length < 2 || tabs.some((tab) => !tab)) {
      return;
    }
    const existingSplitTab = tabs.find((tab) => tab.splitView);
    if (existingSplitTab) {
      const groupIndex = splitViewData().findIndex((group) =>
        group.tabIds.includes(this.getTabId(existingSplitTab)),
      );
      if (groupIndex >= 0) {
        this.updateSplitView(existingSplitTab);
        return;
      }
    }

    setSplitViewData((prev) => {
      prev.push({
        tabIds: tabs.map((tab) => this.setTabId(tab, this.getGeneratedUuid)),
        reverse: false,
        method: "row",
      });
      return prev;
    });

    window.gBrowser.selectedTab = tabs[0];
    this.updateSplitView(tabs[0]);
  }

  /**
   * @description: Add default SplitView. This tab will be used as a fixed tab.
   * @param {Tab} tab - The tab element.
   */
  private splitFixedTab(tab: Tab) {
    if (FixedTabUtils.isWorking()) {
      return;
    }
    setFixedSplitViewData({
      fixedTabId: this.getTabId(tab),
      options: {
        reverse: false,
        method: "row",
      },
    });
  }

  /**
   * @description: Update the split view.
   * @param {Tab} tab - The tab element.
   * @param {boolean | null} reverse - Whether to reverse the split view.
   * @param {string | null} method - The split view method.
   */
  private updateSplitView(
    tab: Tab,
    reverse: boolean | null = null,
    method: "row" | "column" | null = null,
  ) {
    const splitData = this.findSplitDataForTab(tab);
    if (!splitData) {
      this.handleNoSplitData();
      return;
    }

    const newSplitData = this.createNewSplitData(splitData, reverse, method);
    this.updateSplitDataIfNeeded(splitData, newSplitData);

    if (this.shouldDeactivateSplitView(tab)) {
      this.deactivateSplitView();
    }

    this.activateSplitView(newSplitData, tab);
  }

  /**
   * @description: Find the split data for the tab.
   * @param {Tab} tab - The tab element.
   * @returns {SplitViewData | undefined} The split data.
   */
  private findSplitDataForTab(tab: Tab): SplitViewData | undefined {
    return splitViewData().find((group) =>
      group.tabIds.includes(this.getTabId(tab)),
    );
  }

  /**
   * @description: Handle the no split data.
   */
  private handleNoSplitData() {
    if (currentSplitView() >= 0) {
      this.deactivateSplitView();
    }
  }

  /**
   * @description: Create the new split data.
   * @param {SplitViewData} splitData - The split data.
   * @param {boolean | null} reverse - Whether to reverse the split view.
   * @param {string | null} method - The split view method.
   * @returns {SplitViewData} The new split data.
   */
  private createNewSplitData(
    splitData: SplitViewData,
    reverse: boolean | null,
    method: "row" | "column" | null,
  ): SplitViewData {
    return {
      ...splitData,
      method: method === null ? splitData?.method ?? "row" : method,
      reverse: reverse === null ? splitData?.reverse ?? false : reverse,
    };
  }

  /**
   * @description: Update the split data if needed.
   * @param {SplitViewData} oldSplitData - The old split data.
   * @param {SplitViewData} newSplitData - The new split data.
   */
  private updateSplitDataIfNeeded(
    oldSplitData: SplitViewData,
    newSplitData: SplitViewData,
  ) {
    const index = splitViewData().indexOf(oldSplitData);
    if (index >= 0) {
      setSplitViewData((prev) => {
        prev[index] = newSplitData;
        return prev;
      });
    }
  }

  /**
   * @description: Check if the split view should be deactivated.
   * @param {Tab} tab - The tab element.
   * @returns {boolean} Whether the split view should be deactivated.
   */
  private shouldDeactivateSplitView(tab: Tab): boolean {
    return (
      currentSplitView() >= 0 &&
      !splitViewData()[currentSplitView()].tabIds.includes(this.getTabId(tab))
    );
  }

  /**
   * @description: Deactivate the split view.
   */
  private deactivateSplitView() {
    for (const tab of window.gBrowser.tabs) {
      const container = tab.linkedBrowser.closest(".browserSidebarContainer");
      this.resetContainerStyle(container);
      container.removeEventListener("click", this.handleTabClick);
    }
    this.tabBrowserPanel.removeAttribute("split-view");
    this.tabBrowserPanel.style.flexTemplateAreas = "";
    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
    this.setTabsDocShellState(
      splitViewData()[currentSplitView()].tabIds.map((id) =>
        this.getTabById(id),
      ),
      false,
    );
    setCurrentSplitView(-1);
  }

  /**
   * @description: Activate the split view.
   * @param {SplitViewData} splitData - The split data.
   * @param {Tab} activeTab - The active tab element.
   */
  private activateSplitView(splitData: SplitViewData, activeTab: Tab) {
    this.tabBrowserPanel.setAttribute("split-view", "true");
    Services.prefs.setBoolPref("floorp.browser.splitView.working", true);

    setCurrentSplitView(splitViewData().indexOf(splitData));

    this.applyFlexBoxLayout(
      splitData.tabIds.map((id) => this.getTabById(id)),
      activeTab,
      splitData.reverse,
      splitData.method,
    );

    this.setTabsDocShellState(
      splitData.tabIds.map((id) => this.getTabById(id)),
      true,
    );
  }

  /**
   * @description: Apply the flex box layout.
   * @param {Tab[]} tabs - The tab elements.
   * @param {Tab} activeTab - The active tab element.
   * @param {boolean} reverse - Whether to reverse the split view.
   * @param {string} method - The split view method.
   */
  private applyFlexBoxLayout(
    tabs: Tab[],
    activeTab: Tab,
    reverse = true,
    method: "row" | "column" = "column",
  ) {
    this.tabBrowserPanel.style.flexDirection = this.getFlexDirection(
      reverse,
      method,
    );
    tabs.forEach((tab, index) => {
      tab.splitView = true;
      const container = tab.linkedBrowser.closest(
        ".browserSidebarContainer",
      ) as XULElement;
      this.styleContainer(container, tab === activeTab, index);
    });
  }

  /**
   * @description: Get the flex direction.
   * @param {boolean} reverse - Whether to reverse the split view.
   * @param {string} method - The split view method.
   * @returns {string} The flex direction.
   */
  private getFlexDirection(reverse: boolean, method: "row" | "column"): string {
    if (method === "column") {
      return reverse ? "column-reverse" : "column";
    }
    return reverse ? "row-reverse" : "row";
  }

  /**
   * @description: Style the container.
   * @param {XULElement} container - The container element.
   * @param {boolean} isActive - Whether the container is active.
   * @param {number} index - The index.
   */
  private styleContainer(
    container: XULElement,
    isActive: boolean,
    index: number,
  ) {
    container.removeAttribute("split-active");
    if (isActive) {
      container.setAttribute("split-active", "true");
    }
    container.setAttribute("split-anim", "true");
    container.addEventListener("click", this.handleTabClick);

    container.style.flex = "1";
    container.style.order = String(index);
  }

  /**
   * @description: Handle the tab click.
   * @param {Event} event - The event.
   */
  private handleTabClick = (event: Event) => {
    const container = event.currentTarget;
    const tab = window.gBrowser.tabs.find(
      (t: Tab) =>
        t.linkedBrowser.closest(".browserSidebarContainer") === container,
    );
    if (tab) {
      window.gBrowser.selectedTab = tab;
    }
  };

  /**
   * @description: Set the tabs doc shell state.
   * @param {Tab[]} tabs - The tab elements.
   * @param {boolean} active - Whether the tabs should be active.
   */
  private setTabsDocShellState(tabs: Tab[], active: boolean) {
    for (const tab of tabs) {
      tab.linkedBrowser.spliting = active;
      tab.linkedBrowser.docShellIsActive = active;
      const browser = tab.linkedBrowser.closest(
        ".browserSidebarContainer",
      ) as XULElement;

      if (browser) {
        if (active) {
          browser.setAttribute("split", "true");
          const currentStyle = browser.getAttribute("style");
          browser.setAttribute(
            "style",
            `${currentStyle}
            -moz-subtree-hidden-only-visually: 0;
            visibility: visible !important;`,
          );
        } else {
          browser.removeAttribute("split");
          browser.removeAttribute("style");
        }
      }
    }
  }

  /**
   * @description: Reset the container style.
   * @param {XULElement} container - The container element.
   */
  private resetContainerStyle(container: XULElement) {
    container.removeAttribute("split-active");
    container.classList.remove("deck-selected");
    container.style.flex = "";
    container.style.order = "";
  }

  /**
   * @description: Unsplit the current view.
   */
  public unsplitCurrentView() {
    const currentTab = window.gBrowser.selectedTab;
    const tabs = splitViewData()[currentSplitView()].tabIds.map((id) =>
      this.getTabById(id),
    );
    for (const tab of tabs) {
      this.handleTabClose({ target: tab, forUnsplit: true } as TabEvent);
    }
    window.gBrowser.selectedTab = currentTab;
  }
}
