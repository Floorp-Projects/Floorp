import { render } from "@nora/solid-xul";
import type { JSXElement } from "solid-js";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "../../i18n/config.ts";
import { createRootHMR } from "@nora/solid-xul";

export namespace ContextMenuUtils {
  const checkItems: (() => void)[] = [];
  const contextMenuObserver: MutationObserver = new MutationObserver(() => {
    contextMenuObserverFunc();
  });

  function windowModalDialogElem(): XULElement | null {
    return document?.querySelector("#window-modal-dialog") as XULElement | null;
  }
  function screenShotContextMenuItems(): XULElement | null {
    return document?.querySelector(
      "#context-take-screenshot",
    ) as XULElement | null;
  }
  export function contentAreaContextMenu(): XULElement | null {
    return document?.querySelector(
      "#contentAreaContextMenu",
    ) as XULElement | null;
  }
  function pdfjsContextMenuSeparator(): XULElement | null {
    return document?.querySelector(
      "#context-sep-pdfjs-selectall",
    ) as XULElement | null;
  }
  function contextMenuSeparators(): NodeListOf<XULElement> {
    return document?.querySelectorAll(
      "#contentAreaContextMenu > menuseparator",
    ) as NodeListOf<XULElement>;
  }

  export function addContextBox(
    id: string,
    translationKey: string,
    renderElementId: string,
    runFunction: () => void,
    checkID: string,
    checkedFunction: () => void,
  ) {
    const contextMenu = ContextMenu(id, translationKey, runFunction);
    const targetNode = document?.getElementById(checkID) as XULElement;
    const renderElement = document?.getElementById(
      renderElementId,
    ) as XULElement;

    render(() => contextMenu, contentAreaContextMenu(), {
      marker: renderElement,
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
  ) {
    render(JSXElem, document?.body, {
      marker: windowModalDialogElem() ?? undefined,
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
        const nextSibling = contextMenuSeparator.nextSibling as XULElement;

        if (
          nextSibling?.hidden &&
          contextMenuSeparator.id !== "context-sep-navigation" &&
          contextMenuSeparator.id !== "context-sep-pdfjs-selectall"
        ) {
          contextMenuSeparator.hidden = true;
        }
      }
    })();
  }
}

type ContextMenuText = {
  label: string;
};

export function ContextMenu(id: string, translationKey: string, runFunction: () => void) {
  return createRootHMR(
    () => {
      const defaultText: ContextMenuText = {
        label: translationKey
      };

      const [text, setText] = createSignal<ContextMenuText>(defaultText);

      addI18nObserver(() => {
        setText({
          label: i18next.t(translationKey)
        });
      });

      return (
        <xul:menuitem
          label={text().label}
          id={id}
          onCommand={runFunction}
        />
      );
    },
    import.meta.hot,
  );
}
