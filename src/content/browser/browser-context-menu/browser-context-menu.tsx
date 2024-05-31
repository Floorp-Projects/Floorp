import { insert } from "@solid-xul/solid-xul";
import { ContextMenu } from "./context-menu";
import type { JSXElement } from "solid-js";

class gFloorpContextMenuServices {
  initialized = false;
  checkItems: (() => void)[] = [];
  contextMenuObserver: MutationObserver = new MutationObserver(() => {
    this.contextMenuObserverFunc();
  });

  get windowModalDialogElem() {
    return document.getElementById("window-modal-dialog") as XULElement;
  }
  get screenShotContextMenuItems() {
    return document.getElementById("context-take-screenshot") as XULElement;
  }
  get contentAreaContextMenu() {
    return document.getElementById("contentAreaContextMenu") as XULElement;
  }
  get pdfjsContextMenuSeparator() {
    return document.getElementById("context-sep-pdfjs-selectall") as XULElement;
  }
  get contextMenuSeparators(): NodeListOf<XULElement> {
    return document.querySelectorAll("#contentAreaContextMenu > menuseparator");
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.contentAreaContextMenu.addEventListener("popupshowing", () =>
      this.onPopupShowing(),
    );
    this.initialized = true;
  }

  addContextBox(
    id: string,
    l10n: string,
    insertElementId: string,
    runFunction: () => void,
    checkID: string,
    checkedFunction: () => void,
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
  }

  contextMenuObserverFunc() {
    for (const checkItem of this.checkItems) {
      checkItem();
    }
  }

  addToolbarContentMenuPopupSet(JSXElem: JSXElement) {
    insert(document.body, JSXElem, this.windowModalDialogElem);
  }

  onPopupShowing() {
    if (!this.screenShotContextMenuItems.hidden) {
      this.pdfjsContextMenuSeparator.hidden = false;

      const nextSibling = this.screenShotContextMenuItems
        .nextSibling as XULElement;
      if (nextSibling) {
        nextSibling.hidden = false;
      }
    }

    (async () => {
      for (const contextMenuSeparator of this.contextMenuSeparators) {
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
  }
}

export const gFloorpContextMenu = new gFloorpContextMenuServices();
