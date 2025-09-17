/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTICE: Do not add toolbar buttons code here. Create new folder or file for new toolbar buttons.

import { render } from "@nora/solid-xul";
import { createRoot, getOwner, type JSXElement } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

class WidgetDeletionTracker {
  private static readonly PREF_KEY = "floorp.browser.deletedWidgets";
  private static instance: WidgetDeletionTracker;

  private constructor() {
    this.initializeListener();
  }

  static getInstance(): WidgetDeletionTracker {
    if (!this.instance) {
      this.instance = new WidgetDeletionTracker();
    }
    return this.instance;
  }

  private getDeletedWidgets(): Set<string> {
    try {
      const deletedJson = Services.prefs.getStringPref(
        WidgetDeletionTracker.PREF_KEY,
        "[]",
      );
      return new Set(JSON.parse(deletedJson));
    } catch {
      return new Set();
    }
  }

  private saveDeletedWidgets(deletedWidgets: Set<string>): void {
    Services.prefs.setStringPref(
      WidgetDeletionTracker.PREF_KEY,
      JSON.stringify([...deletedWidgets]),
    );
  }

  addDeletedWidget(widgetId: string): void {
    const deletedWidgets = this.getDeletedWidgets();
    deletedWidgets.add(widgetId);
    this.saveDeletedWidgets(deletedWidgets);
  }

  removeDeletedWidget(widgetId: string): void {
    const deletedWidgets = this.getDeletedWidgets();
    deletedWidgets.delete(widgetId);
    this.saveDeletedWidgets(deletedWidgets);
  }

  isWidgetDeleted(widgetId: string): boolean {
    return this.getDeletedWidgets().has(widgetId);
  }

  private initializeListener(): void {
    if (globalThis.floorpDeletionListenerAdded) {
      return;
    }

    const deletionListener = {
      onWidgetRemoved: (aWidgetId: string) => {
        this.addDeletedWidget(aWidgetId);
      },
      onWidgetAdded: (aWidgetId: string) => {
        this.removeDeletedWidget(aWidgetId);
      },
    };

    CustomizableUI.addListener(deletionListener);
    globalThis.floorpDeletionListenerAdded = true;
  }
}

const widgetTracker = WidgetDeletionTracker.getInstance();

class ToolbarWidgetCreator {
  static renderStyleElement(styleElement: JSXElement | null): void {
    if (styleElement) {
      render(() => styleElement, document?.head, {
        marker: document?.head?.lastChild as Element,
      });
    }
  }

  private static shouldAddWidgetToArea(widgetId: string): boolean {
    return (
      !widgetTracker.isWidgetDeleted(widgetId) &&
      !CustomizableUI.getPlacementOfWidget(widgetId)
    );
  }

  private static addWidgetToAreaIfNeeded(
    widgetId: string,
    area: string,
    position: number | null,
  ): void {
    if (this.shouldAddWidgetToArea(widgetId)) {
      CustomizableUI.addWidgetToArea(
        widgetId,
        area,
        position ?? 0,
      );
    }
  }
}

export namespace BrowserActionUtils {
  export function createToolbarClickActionButton(
    widgetId: string,
    l10nId: string | null,
    onCommandFunc: (event: XULCommandEvent) => void,
    styleElement: JSXElement | null = null,
    area: string = CustomizableUI.AREA_NAVBAR,
    position: number | null = 0,
    onCreatedFunc: null | ((aNode: XULElement) => void) = null,
  ) {
    ToolbarWidgetCreator.renderStyleElement(styleElement);

    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }

    (async () => {
      const tooltiptext = l10nId
        ? await document?.l10n?.formatValue(l10nId)
        : null;
      const label = l10nId ? await document?.l10n?.formatValue(l10nId) : null;

      CustomizableUI.createWidget({
        id: widgetId,
        type: "button",
        tooltiptext,
        label,
        removable: true,
        onCommand: (event: XULCommandEvent) => {
          onCommandFunc?.(event);
        },
        onCreated: (aNode: XULElement) => {
          onCreatedFunc?.(aNode);
          ToolbarWidgetCreator.addWidgetToAreaIfNeeded(
            widgetId,
            area,
            position,
          );
        },
      });

      ToolbarWidgetCreator.addWidgetToAreaIfNeeded(widgetId, area, position);
    })();
  }

  export function createMenuToolbarButton(
    widgetId: string,
    l10nId: string,
    targetViewId: string,
    popupElement: JSXElement,
    onViewShowingFunc?: ((event: Event) => void) | null,
    onCreatedFunc?: (aNode: XULElement) => void,
    area: string = CustomizableUI.AREA_NAVBAR,
    styleElement: JSXElement | null = null,
    position: number | null = 0,
  ) {
    ToolbarWidgetCreator.renderStyleElement(styleElement);

    if (popupElement) {
      render(() => popupElement, document?.getElementById("mainPopupSet"), {
        marker: document?.getElementById("mainPopupSet")?.lastChild as Element,
      });
    }

    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }

    const owner = getOwner();
    (async () => {
      const tooltiptext = await document?.l10n?.formatValue(l10nId) ?? "";
      const label = await document?.l10n?.formatValue(l10nId) ?? "";

      CustomizableUI.createWidget({
        id: widgetId,
        type: "view",
        viewId: targetViewId,
        tooltiptext,
        label,
        removable: true,
        onCreated: (aNode: XULElement) => {
          createRoot(() => onCreatedFunc?.(aNode), owner);
          ToolbarWidgetCreator.addWidgetToAreaIfNeeded(
            widgetId,
            area,
            position,
          );
        },
        onViewShowing: (event: Event) => {
          createRoot(() => onViewShowingFunc?.(event), owner);
        },
      });

      ToolbarWidgetCreator.addWidgetToAreaIfNeeded(widgetId, area, position);
    })();
  }
}
