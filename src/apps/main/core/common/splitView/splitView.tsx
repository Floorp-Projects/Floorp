/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SplitViewStaticNames } from "./utils/static-names";
import { render } from "@nora/solid-xul";
import splitViewStyles from "./style.css?inline";
import {
  currentSplitView,
  setCurrentSplitView,
  setSplitViewData,
  splitViewData,
} from "./utils/data";
import type { Browser, SplitViewData, Tab, TabEvent } from "./utils/type";

export class SplitView {
  private static instance: SplitView;
  static getInstance() {
    if (!SplitView.instance) {
      SplitView.instance = new SplitView();
    }
    return SplitView.instance;
  }

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

  private listener = {
    onLocationChange: (browser: Browser) => {
      this.checkAllTabHaveSplitViewAttribute();
      const tab = window.gBrowser.selectedTab;
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

  private StyleElement() {
    return (<style>{splitViewStyles}</style>) as Element;
  }

  get tabBrowserPanel() {
    return document?.getElementById("tabbrowser-tabpanels") as XULElement;
  }

  get modifierElement() {
    const wrapper = document?.getElementById(
      "template-split-view-modifier",
    ) as XULElement & { content: XULElement };
    const panel = (wrapper?.content as XULElement).firstElementChild;
    wrapper?.replaceWith(wrapper.content);
    return panel as XULElement;
  }

  private get getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  private getTabId(tab: Tab) {
    if (!tab) {
      throw new Error("Tab is not defined");
    }
    return tab.getAttribute(SplitViewStaticNames.TabAttributionId) ?? "";
  }

  private setTabId(tab: Tab, id: string) {
    const currentId = tab.getAttribute(SplitViewStaticNames.TabAttributionId);
    if (currentId) {
      return currentId;
    }

    tab.setAttribute(SplitViewStaticNames.TabAttributionId, id);
    return id;
  }

  private getTabById(id: string) {
    return document?.querySelector(
      `[${SplitViewStaticNames.TabAttributionId}="${id}"]`,
    ) as Tab;
  }

  private checkAllTabHaveSplitViewAttribute() {
    for (const tab of window.gBrowser.tabs) {
      if (!tab.getAttribute(SplitViewStaticNames.TabAttributionId)) {
        this.setTabId(tab, this.getGeneratedUuid);
      }
    }
  }

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

  public handleSplitViewPanelRevseOptionClick(reverse: boolean) {
    this.updateSplitView(window.gBrowser.selectedTab, reverse === true, null);
  }

  public handleSplitViewPanelTypeOptionClick(method: "row" | "column") {
    this.updateSplitView(window.gBrowser.selectedTab, null, method);
  }

  private addMainPopupShowingListener() {
    const mainPopup = document?.getElementById("mainPopupSet");
    mainPopup?.addEventListener("popupshowing", () => {
      if (!window.TabContextMenu.contextTab) {
        return;
      }

      const elem = document?.getElementById("context_splittabs") as XULElement;
      if (elem) {
        const isDisabled =
          window.gBrowser.selectedTab === window.TabContextMenu.contextTab ||
          splitViewData().some((group) =>
            group.tabIds.includes(
              this.getTabId(window.TabContextMenu.contextTab),
            ),
          ) ||
          splitViewData().some((group) =>
            group.tabIds.includes(this.getTabId(window.gBrowser.selectedTab)),
          );

        elem.setAttribute("disabled", String(isDisabled));
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

  public contextSplitTabs() {
    if (window.TabContextMenu.contextTab === window.gBrowser.selectedTab) {
      return;
    }
    const tab = [window.TabContextMenu.contextTab, window.gBrowser.selectedTab];
    this.splitTabs(tab);
  }

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

  private findSplitDataForTab(tab: Tab): SplitViewData | undefined {
    return splitViewData().find((group) =>
      group.tabIds.includes(this.getTabId(tab)),
    );
  }

  private handleNoSplitData() {
    if (currentSplitView() >= 0) {
      this.deactivateSplitView();
    }
  }

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

  private updateSplitDataIfNeeded(
    oldSplitData: SplitViewData,
    newSplitData: SplitViewData,
  ) {
    const index = splitViewData().indexOf(oldSplitData);
    if (index >= 0) {
      splitViewData()[index] = newSplitData;
    }
  }

  private shouldDeactivateSplitView(tab: Tab): boolean {
    return (
      currentSplitView() >= 0 &&
      !splitViewData()[currentSplitView()].tabIds.includes(this.getTabId(tab))
    );
  }

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

  private getFlexDirection(reverse: boolean, method: "row" | "column") {
    if (method === "column") {
      return reverse ? "column-reverse" : "column";
    }
    return reverse ? "row-reverse" : "row";
  }

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

  private resetContainerStyle(container: XULElement) {
    container.removeAttribute("split-active");
    container.classList.remove("deck-selected");
    container.style.flex = "";
    container.style.order = "";
  }

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
