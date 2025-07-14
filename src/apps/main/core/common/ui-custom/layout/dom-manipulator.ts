/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { config } from "@core/common/designs/configs.ts";

export class DOMLayoutManager {
  private static readonly DEBUG_PREFIX = "[DOMLayoutManager]";

  // Store original navbar position for proper restoration
  private originalNavbarParent: Element | null = null;
  private originalNavbarNextSibling: Element | null = null;
  private isNavbarAtBottom = false;

  private get navBar(): XULElement {
    const element = document?.getElementById("nav-bar") as XULElement;
    return element;
  }

  private get fullscreenWrapper(): XULElement {
    const element = document?.getElementById(
      "a11y-announcement",
    ) as XULElement;
    return element;
  }

  private get navigatorToolbox(): XULElement {
    const element = document?.getElementById("navigator-toolbox") as XULElement;
    return element;
  }

  private get personalToolbar(): XULElement {
    const element = document?.getElementById("PersonalToolbar") as XULElement;
    return element;
  }

  setupDOMEffects() {
    this.setupNavbarPosition();
  }

  /**
   * Reset the saved navbar position state
   * Useful when the DOM structure changes or for cleanup
   */
  private resetNavbarPositionState() {
    this.originalNavbarParent = null;
    this.originalNavbarNextSibling = null;
    this.isNavbarAtBottom = false;
  }

  private setupNavbarPosition() {
    createEffect(() => {
      const currentPosition = config().uiCustomization.navbar.position;
      try {
        if (currentPosition === "bottom") {
          this.moveNavbarToBottom();
        } else {
          this.moveNavbarToTop();
        }
      } catch (error: unknown) {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error in navbar position setup:`,
          error,
        );
      }
    });
  }

  private moveNavbarToBottom() {
    const navbar = this.navBar;
    const fullscreenWrapper = this.fullscreenWrapper;

    if (!navbar) {
      console.warn(`${DOMLayoutManager.DEBUG_PREFIX} navbar element not found`);
      return;
    }

    if (!fullscreenWrapper) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} fullscreenWrapper element not found`,
      );
      return;
    }

    if (this.isNavbarAtBottom) {
      return;
    }

    try {
      this.originalNavbarParent = navbar.parentElement;
      this.originalNavbarNextSibling = navbar.nextElementSibling;

      fullscreenWrapper.after(navbar);
      this.isNavbarAtBottom = true;

      this.fixUrlbarInputContainer();
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error in moveNavbarToBottom:`,
        error,
      );
    }
  }

  private moveNavbarToTop() {
    // When changing from bottom to top position, a restart is required
    // to properly restore the original DOM structure
    if (this.isNavbarAtBottom) {
      console.info(
        `${DOMLayoutManager.DEBUG_PREFIX} Navbar position change requires restart to take effect`,
      );

      // Optionally show a notification to the user about restart requirement
      // This could be implemented with a toast notification or similar UI element

      return;
    }

    // If navbar is already at top, no action needed
  }

  private fixUrlbarInputContainer() {
    // Wait for SessionStore to be initialized
    const sessionStore = globalThis.SessionStore;

    if (!sessionStore) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} SessionStore not available`,
      );
      return;
    }

    if (sessionStore?.promiseInitialized) {
      sessionStore.promiseInitialized.then(() => {
        const urlbarView = document?.querySelector(".urlbarView");
        const urlbarInputContainer = document?.querySelector(
          ".urlbar-input-container",
        );

        if (!urlbarView) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} urlbarView element not found`,
          );
          return;
        }

        if (!urlbarInputContainer) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} urlbarInputContainer element not found`,
          );
          return;
        }

        try {
          urlbarView.after(urlbarInputContainer);
        } catch (error: unknown) {
          console.error(
            `${DOMLayoutManager.DEBUG_PREFIX} Error moving urlbar input container:`,
            error,
          );
        }
      }).catch((error: unknown) => {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error waiting for SessionStore initialization:`,
          error,
        );
      });
    } else {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} SessionStore.promiseInitialized not available`,
      );
    }
  }

  private restoreUrlbarInputContainer() {
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );

    if (!urlbarView) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} urlbarView element not found`,
      );
      return;
    }

    if (!urlbarInputContainer) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} urlbarInputContainer element not found`,
      );
      return;
    }

    try {
      urlbarView.before(urlbarInputContainer);
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error restoring urlbar input container:`,
        error,
      );
    }
  }
}
