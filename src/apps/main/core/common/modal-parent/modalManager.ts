/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  isModalVisible,
  type ModalSize,
  setModalSize,
  setModalVisible,
} from "./data/data.ts";

export class ModalManager {
  private static readonly targetParent = document?.getElementById(
    "tabbrowser-tabbox",
  ) as HTMLElement;

  constructor() {
    const handleKeydown = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isModalVisible()) {
        this.hide();
      }
    };
    globalThis.addEventListener("keydown", handleKeydown);
    onCleanup(() => globalThis.removeEventListener("keydown", handleKeydown));
  }

  public show(): void {
    const browser = document?.getElementById(
      "modal-child-browser",
    ) as XULElement;
    if (browser) {
      setModalVisible(true);

      if (
        browser.getAttribute("src") !==
          "chrome://noraneko-modal-child/content/index.html"
      ) {
        browser.setAttribute(
          "src",
          "chrome://noraneko-modal-child/content/index.html",
        );
      }

      browser.focus();
    }
  }

  public hide(): void {
    const browser = document?.getElementById(
      "modal-child-browser",
    ) as XULElement;
    if (browser) {
      setModalVisible(false);

      browser.setAttribute("src", "about:blank");

      setModalSize({ maxWidth: 600, maxHeight: 800 });

      globalThis.focus();
    }
  }

  public setModalSize(newSize: ModalSize): void {
    setModalSize((current) => ({ ...current, ...newSize }));
  }

  public handleBackdropClick(event: MouseEvent): void {
    const target = event.target as HTMLElement;
    if (target.id === "modal-parent-container") {
      this.hide();
    }
  }

  public static get parentElement(): HTMLElement {
    return this.targetParent;
  }
}
