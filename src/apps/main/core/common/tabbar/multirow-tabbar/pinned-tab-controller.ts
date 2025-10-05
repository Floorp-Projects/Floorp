import { findChildIndex } from "./dom-utils.ts";

export class PinnedTabController {
  private mutationObserver: MutationObserver | null = null;
  private isRegistered = false;

  constructor(
    private readonly resolveTabsContainer: () => XULElement | null,
  ) {}

  register(): void {
    if (this.isRegistered) return;

    const tabsContainer = this.resolveTabsContainer();
    const pinnedTabsContainer = document?.getElementById(
      "pinned-tabs-container",
    );

    if (!tabsContainer || !pinnedTabsContainer) {
      return;
    }

    this.mutationObserver = new MutationObserver((mutationList) => {
      for (const mutation of mutationList) {
        this.migratePinnedTabs(
          tabsContainer,
          mutation.addedNodes as NodeListOf<Element>,
        );
      }
    });

    this.mutationObserver.observe(pinnedTabsContainer, { childList: true });

    gBrowser.tabContainer.addEventListener(
      "TabUnpinned",
      this.handleTabUnpinned,
      false,
    );

    this.isRegistered = true;
  }

  unregister(): void {
    if (!this.isRegistered) return;

    if (this.mutationObserver) {
      this.mutationObserver.disconnect();
      this.mutationObserver = null;
    }

    gBrowser.tabContainer.removeEventListener(
      "TabUnpinned",
      this.handleTabUnpinned,
      false,
    );

    this.isRegistered = false;
  }

  migratePinnedTabs(
    newContainer: Element,
    pinnedTabs: NodeListOf<Element>,
  ): void {
    if (!pinnedTabs || pinnedTabs.length === 0) return;

    pinnedTabs.forEach((tab: Element) => {
      tab.setAttribute("newPin", "true");

      const firstUnpinnedTab = newContainer.querySelector(
        ".tabbrowser-tab:not([pinned])",
      );
      const periphery = document?.getElementById(
        "tabbrowser-arrowscrollbox-periphery",
      );

      if (firstUnpinnedTab) {
        newContainer.insertBefore(tab, firstUnpinnedTab);
      } else if (periphery) {
        newContainer.insertBefore(tab, periphery);
      }
    });
  }

  private handleTabUnpinned = (event: Event): void => {
    this.fixUnpinnedTabsPosition(event);
  };

  private fixUnpinnedTabsPosition(event: Event): void {
    const tab = event.target as Element;
    tab.removeAttribute("newPin");

    const tabsContainer = this.resolveTabsContainer();
    if (!tabsContainer) return;

    const pinnedTabs = tabsContainer.querySelectorAll(
      ".tabbrowser-tab[pinned]",
    );
    if (!pinnedTabs || pinnedTabs.length === 0) return;

    const lastPinnedTab = pinnedTabs[pinnedTabs.length - 1];
    const indexToInsertBefore = findChildIndex(tabsContainer, lastPinnedTab) +
      1;

    tabsContainer.insertBefore(
      tab,
      tabsContainer.childNodes[indexToInsertBefore],
    );
  }
}
