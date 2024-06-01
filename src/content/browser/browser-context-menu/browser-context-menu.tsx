import { insert } from "@solid-xul/solid-xul";
import { ContextMenu } from "./context-menu";
import type { JSXElement } from "solid-js";

class gFloorpContextMenuServices {
  private static instance: gFloorpContextMenuServices;
  private initialized = false;
  private checkItems: (() => void)[] = [];
  private contextMenuObserver: MutationObserver = new MutationObserver(() => {
    this.contextMenuObserverFunc();
  });

  static getInstance(): gFloorpContextMenuServices {
    if (!gFloorpContextMenuServices.instance) {
      gFloorpContextMenuServices.instance = new gFloorpContextMenuServices();
    }
    return gFloorpContextMenuServices.instance;
  }

  private get windowModalDialogElem() {
    return document.getElementById("window-modal-dialog") as XULElement;
  }
  private get screenShotContextMenuItems() {
    return document.getElementById("context-take-screenshot") as XULElement;
  }
  private get contentAreaContextMenu() {
    return document.getElementById("contentAreaContextMenu") as XULElement;
  }
  private get pdfjsContextMenuSeparator() {
    return document.getElementById("context-sep-pdfjs-selectall") as XULElement;
  }
  private get contextMenuSeparators(): NodeListOf<XULElement> {
    return document.querySelectorAll("#contentAreaContextMenu > menuseparator");
  }

  public init() {
    if (this.initialized) {
      return;
    }
    this.contentAreaContextMenu.addEventListener("popupshowing", () =>
      this.onPopupShowing(),
    );
    this.initialized = true;
  }

  public addContextBox(
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

  private contextMenuObserverFunc() {
    for (const checkItem of this.checkItems) {
      checkItem();
    }
  }

  public addToolbarContentMenuPopupSet(JSXElem: JSXElement) {
    insert(document.body, JSXElem, this.windowModalDialogElem);
  }

  private onPopupShowing() {
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

export const gFloorpContextMenu = gFloorpContextMenuServices.getInstance();
