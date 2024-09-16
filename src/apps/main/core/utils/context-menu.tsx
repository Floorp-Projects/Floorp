import { insert, render } from "@nora/solid-xul";
import type { JSXElement } from "solid-js";
import type { ViteHotContext } from "vite/types/hot";

export namespace ContextMenuUtils {
  const checkItems: (() => void)[] = [];
  const contextMenuObserver: MutationObserver = new MutationObserver(() => {
    contextMenuObserverFunc();
  });

  function windowModalDialogElem(): XULElement | null {
    return document.querySelector("#window-modal-dialog");
  }
  function screenShotContextMenuItems(): XULElement | null {
    return document.querySelector("#context-take-screenshot");
  }
  export function contentAreaContextMenu(): XULElement | null {
    return document.querySelector("#contentAreaContextMenu");
  }
  function pdfjsContextMenuSeparator(): XULElement | null {
    return document.querySelector("#context-sep-pdfjs-selectall");
  }
  function contextMenuSeparators(): NodeListOf<XULElement> {
    return document.querySelectorAll("#contentAreaContextMenu > menuseparator");
  }

  export function addContextBox(
    id: string,
    l10n: string,
    insertElementId: string,
    runFunction: () => void,
    checkID: string,
    checkedFunction: () => void,
    hotCtx?: ViteHotContext,
  ) {
    const contextMenu = ContextMenu(id, l10n, runFunction);
    const targetNode = document.getElementById(checkID) as XULElement;
    const insertElement = document.getElementById(
      insertElementId,
    ) as XULElement;

    render(() => contextMenu, contentAreaContextMenu(), {
      marker: insertElement,
      hotCtx: hotCtx,
    });
    contextMenuObserver.observe(targetNode, { attributes: true });
    checkItems.push(checkedFunction);
    contextMenuObserverFunc();
  }

  function contextMenuObserverFunc() {
    for (const checkItem of checkItems) {
      checkItem();
    }
  }

  export function addToolbarContentMenuPopupSet(
    JSXElem: () => JSXElement,
    hotCtx?: ViteHotContext,
  ) {
    render(JSXElem, document.body, {
      marker: windowModalDialogElem() ?? undefined,
      hotCtx,
    });
  }

  export function onPopupShowing() {
    console.log("onpopupshowing");
    if (!screenShotContextMenuItems()?.hidden) {
      const sep = pdfjsContextMenuSeparator();
      if (sep) sep.hidden = false;

      const nextSibling = screenShotContextMenuItems()
        ?.nextSibling as XULElement;
      if (nextSibling) nextSibling.hidden = false;
    }

    (async () => {
      for (const contextMenuSeparator of contextMenuSeparators()) {
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

export function ContextMenu(id: string, l10n: string, runFunction: () => void) {
  return (
    <xul:menuitem
      data-l10n-id={l10n}
      label={l10n}
      id={id}
      onCommand={runFunction}
    />
  );
}
