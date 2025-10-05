export interface XULTab extends XULElement {
  screenX: number;
  screenY: number;
  container?: XULElement;
  hasAttribute(name: string): boolean;
  removeAttribute(name: string): void;
  pinned?: boolean;
  style: CSSStyleDeclaration;
}

export interface TabBrowser {
  tabContainer: TabContainer;
  selectedTab: XULTab;
  visibleTabs: XULTab[];
  selectedTabs: XULTab[];
  pinTab(tab: XULTab): void;
  unpinTab(tab: XULTab): void;
  moveTabBefore(tab: XULTab | XULElement, beforeTab: XULTab | XULElement): void;
  moveTabAfter(tab: XULTab | XULElement, afterTab: XULTab | XULElement): void;
  moveTabToGroup(tab: XULTab, group: XULElement): void;
}

export interface TabContainer extends XULElement {
  arrowScrollbox: XULElement & { clientHeight: number };
  verticalMode?: boolean;
  _dragTime?: number;
  _dragOverDelay?: number;
  selectedItem?: XULElement;
  _getDropIndex?: (event: DragEvent) => number;
  getDropEffectForTabDrag?: (event: DragEvent) => string;
  _getDropEffectForTabDrag?: (event: DragEvent) => string;
  on_dragover?: (event: DragEvent) => void;
  _onDragOver?: (event: DragEvent) => void;
}

export interface FirefoxWindow {
  gBrowser: TabBrowser;
  gMultiProcessBrowser: boolean;
  gFissionBrowser: boolean;
  isChromeWindow: boolean;
  getComputedStyle(
    element: Element,
  ): CSSStyleDeclaration & { direction: string };
}

export interface PrivateBrowsingUtilsType {
  isWindowPrivate(window: FirefoxWindow | Window): boolean;
}

export interface ServicesType {
  droppedLinkHandler: {
    canDropLink(event: DragEvent, allowSameDocument: boolean): boolean;
  };
}

declare const PrivateBrowsingUtils: PrivateBrowsingUtilsType;
declare const TAB_DROP_TYPE: string;
// deno-lint-ignore no-explicit-any
declare function makeURI(uri: string): any;
