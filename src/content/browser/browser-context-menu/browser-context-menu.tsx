import { insert } from "@solid-xul/solid-xul";
import { ContextMenu } from "./context-menu";
import type { JSXElement } from "solid-js";

export const gFloorpContextMenu: {
  initialized: boolean;
  checkItems: (() => void)[];
  contextMenuObserver: MutationObserver;
  windowModalDialogElem: XULElement;
  screenShotContextMenuItems: XULElement;
  contentAreaContextMenu: XULElement;
  pdfjsContextMenuSeparator: XULElement;
  contextMenuSeparators: NodeListOf<XULElement>;
  init: () => void;
  addContextBox: (
    id: string,
    l10n: string,
    insert: string,
    runFunction: () => void,
    checkID: string,
    checkedFunction: () => void,
  ) => void;
  contextMenuObserverFunc: () => void;
  addToolbarContentMenuPopupSet: (xulElement: JSXElement) => void;
  onPopupShowing: () => void;
} = {
  initialized: false,
  checkItems: [],
  contextMenuObserver: new MutationObserver(() =>
    gFloorpContextMenu.contextMenuObserverFunc(),
  ),

  get windowModalDialogElem() {
    return document.getElementById("window-modal-dialog") as XULElement;
  },
  get screenShotContextMenuItems() {
    return document.getElementById("context-take-screenshot") as XULElement;
  },
  get contentAreaContextMenu() {
    return document.getElementById("contentAreaContextMenu") as XULElement;
  },
  get pdfjsContextMenuSeparator() {
    return document.getElementById("context-sep-pdfjs-selectall") as XULElement;
  },
  get contextMenuSeparators() {
    return document.querySelectorAll(
      "#contentAreaContextMenu > menuseparator",
    ) as NodeListOf<XULElement>;
  },

  init() {
    if (this.initialized) {
      return;
    }
    gFloorpContextMenu.contentAreaContextMenu.addEventListener(
      "popupshowing",
      gFloorpContextMenu.onPopupShowing,
    );
    this.initialized = true;
  },

  addContextBox(
    id,
    l10n,
    insertElementId,
    runFunction,
    checkID,
    checkedFunction,
  ) {
    const contextMenu = ContextMenu(id, l10n, runFunction);
    const targetNode = document.getElementById(checkID) as XULElement;
    const insertElement = document.getElementById(
      insertElementId,
    ) as XULElement;

    insert(this.contentAreaContextMenu, () => contextMenu, insertElement);
    this.contextMenuObserver.observe(targetNode, { attributes: true });
    this.checkItems.push(checkedFunction);
    this.contextMenuObserverFunc();
  },

  contextMenuObserverFunc() {
    for (const checkItem of this.checkItems) {
      checkItem();
    }
  },

  addToolbarContentMenuPopupSet(JSXElem) {
    insert(document.body, JSXElem, this.windowModalDialogElem);
  },

  onPopupShowing() {
    if (!gFloorpContextMenu.screenShotContextMenuItems.hidden) {
      gFloorpContextMenu.pdfjsContextMenuSeparator.hidden = false;

      const nextSibling = gFloorpContextMenu.screenShotContextMenuItems
        .nextSibling as XULElement;
      if (nextSibling) {
        nextSibling.hidden = false;
      }
    }

    (async () => {
      for (const contextMenuSeparator of gFloorpContextMenu.contextMenuSeparators) {
        if (
          contextMenuSeparator.nextSibling.hidden &&
          contextMenuSeparator.previousSibling.hidden &&
          contextMenuSeparator.id !== "context-sep-navigation" &&
          contextMenuSeparator.id !== "context-sep-pdfjs-selectall"
        ) {
          contextMenuSeparator.hidden = true;
        }
      }
    })();
  },
};
