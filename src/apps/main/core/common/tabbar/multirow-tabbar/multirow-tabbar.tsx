/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs.ts";
import { createEffect } from "solid-js";

export class MultirowTabbarClass {
  private get arrowScrollbox(): XULElement | null {
    return document?.querySelector("#tabbrowser-arrowscrollbox") || null;
  }

  private get scrollboxPart(): XULElement | null {
    return this.arrowScrollbox
      ? this.arrowScrollbox.shadowRoot?.querySelector(
        "[part='items-wrapper']",
      ) || null
      : null;
  }

  private get aTabHeight(): number {
    return (
      document?.querySelector(".tabbrowser-tab:not([hidden='true'])")
        ?.clientHeight || 30
    );
  }

  private get isMaxRowEnabled(): boolean {
    return config().tabbar.multiRowTabBar.maxRowEnabled;
  }
  private get getMultirowTabMaxHeight(): number {
    return config().tabbar.multiRowTabBar.maxRow * this.aTabHeight;
  }

  private draggedTabElement: Element | null = null; // Track the dragged tab

  private setMultirowTabMaxHeight() {
    if (!this.isMaxRowEnabled) {
      return;
    }
    this.scrollboxPart?.setAttribute(
      "style",
      `max-height: ${this.getMultirowTabMaxHeight}px !important;`,
    );
    this.arrowScrollbox?.style.setProperty(
      "max-height",
      `${this.getMultirowTabMaxHeight}px`,
    );
  }

  private removeMultirowTabMaxHeight() {
    this.scrollboxPart?.removeAttribute("style");
    this.arrowScrollbox?.style.removeProperty("max-height");
  }

  private preventDrag(event: DragEvent) {
    event.preventDefault();
  }

  /*
  private disableDrag() {
    this.arrowScrollbox?.addEventListener("dragstart", this.preventDrag, true);
    this.arrowScrollbox?.addEventListener("drop", this.preventDrag, true);
  }

  private enableDrag() {
    this.arrowScrollbox?.removeEventListener(
      "dragstart",
      this.preventDrag,
      true,
    );
    this.arrowScrollbox?.removeEventListener("drop", this.preventDrag, true);
  }
  */

  // --- Custom Drag Handlers ---
  private handleDragStart(event: DragEvent) {
    const target = event.target as Element;
    const tab = target.closest(".tabbrowser-tab");
    if (!tab || !(event.dataTransfer)) {
      return;
    }
    event.stopPropagation(); // Prevent event from bubbling further or reaching default handlers (Capture Phase)
    this.draggedTabElement = tab;
    // Use a minimal data transfer, can be enhanced later
    try { // Added try-catch as stopPropagation might interfere in some edge cases
      event.dataTransfer.setData("text/plain", tab.id || "dragging-tab");
      event.dataTransfer.effectAllowed = "move";
    } catch (e) {
      console.error("Error setting drag data in handleDragStart:", e);
      return; // Stop if data transfer fails
    }
    tab.classList.add("floorp-tab-dragging"); // Add class for visual feedback
    console.log("Custom Drag Start (Capture):", tab);
  }

  private handleDragOver(event: DragEvent) {
    console.log("Custom Drag Over:", event.target);
    if (!this.draggedTabElement) {
      console.log("Drag Over: No dragged element, returning.");
      return; // Only react if a custom drag is in progress
    }
    event.preventDefault(); // Necessary to allow dropping
    event.stopPropagation(); // Prevent Firefox default handling
    event.dataTransfer!.dropEffect = "move";

    // Optional: Add visual feedback to the potential drop target
    const target = event.target as Element;
    const potentialTargetTab = target.closest(".tabbrowser-tab");
    // Remove indicator from previous potential targets
    this.arrowScrollbox?.querySelectorAll(".floorp-tab-dragover").forEach((
      el: Element,
    ) => el.classList.remove("floorp-tab-dragover"));
    if (potentialTargetTab && potentialTargetTab !== this.draggedTabElement) {
      potentialTargetTab.classList.add("floorp-tab-dragover");
    }
  }

  private handleDrop(event: DragEvent) {
    console.log("Custom Drop Attempt:", event.target);
    if (!this.draggedTabElement) {
      console.log("Drop: No dragged element, returning.");
      return; // Only react if a custom drag is in progress
    }
    event.preventDefault(); // Prevent default browser drop action
    event.stopPropagation(); // Prevent Firefox default handling
    const target = event.target as Element;
    const targetTab = target.closest(".tabbrowser-tab");

    // Clean up visual feedback regardless of drop target validity
    this.draggedTabElement.classList.remove("floorp-tab-dragging");
    this.arrowScrollbox?.querySelectorAll(".floorp-tab-dragover").forEach((
      el: Element,
    ) => el.classList.remove("floorp-tab-dragover"));

    if (targetTab && targetTab !== this.draggedTabElement) {
      console.log("Custom Drop:", this.draggedTabElement, "onto", targetTab);
      this.onCustomDrop(targetTab); // Call the placeholder
    } else {
      console.log("Custom Drop: Invalid target or dropped on self");
    }

    this.draggedTabElement = null; // Reset dragged element
  }

  private handleDragEnd(event: DragEvent) {
    // Clean up visual feedback if drag ends unexpectedly (e.g., Escape key)
    if (this.draggedTabElement) {
      this.draggedTabElement.classList.remove("floorp-tab-dragging");
      this.draggedTabElement = null;
    }
    this.arrowScrollbox?.querySelectorAll(".floorp-tab-dragover").forEach((
      el: Element,
    ) => el.classList.remove("floorp-tab-dragover"));
    console.log("Custom Drag End");
  }

  // Placeholder for user's drop logic
  private onCustomDrop(targetTabElement: Element) {
    // User will implement the actual reordering logic here
    console.log("onCustomDrop called with target:", targetTabElement);
  }
  // --- End Custom Drag Handlers ---

  private enableMultirowTabbar() {
    this.scrollboxPart?.setAttribute("style", "flex-wrap: wrap;");
    // REMOVED: this.disableDrag();

    // Add custom drag listeners using CAPTURE phase on the arrowScrollbox
    this.arrowScrollbox?.addEventListener(
      "dragstart",
      this.handleDragStart.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.addEventListener(
      "dragover",
      this.handleDragOver.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.addEventListener(
      "drop",
      this.handleDrop.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.addEventListener(
      "dragend",
      this.handleDragEnd.bind(this),
      true,
    ); // Use capture
    console.log(
      "Multirow enabled, custom drag listeners added (capture phase).",
    );
  }

  private disableMultirowTabbar() {
    this.scrollboxPart?.removeAttribute("style");
    // REMOVED: this.enableDrag();

    // Remove custom drag listeners (must match capture phase)
    this.arrowScrollbox?.removeEventListener(
      "dragstart",
      this.handleDragStart.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.removeEventListener(
      "dragover",
      this.handleDragOver.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.removeEventListener(
      "drop",
      this.handleDrop.bind(this),
      true,
    ); // Use capture
    this.arrowScrollbox?.removeEventListener(
      "dragend",
      this.handleDragEnd.bind(this),
      true,
    ); // Use capture
    console.log(
      "Multirow disabled, custom drag listeners removed (capture phase).",
    );
  }

  constructor() {
    createEffect(() => {
      if (config().tabbar.tabbarStyle === "multirow") {
        this.enableMultirowTabbar();
        this.setMultirowTabMaxHeight();
      } else {
        this.disableMultirowTabbar();
        this.removeMultirowTabMaxHeight();
      }
    });
  }
}
