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
  private urlbarFixIntervalId: number | null = null;
  private urlbarMutationObserver: MutationObserver | null = null;
  private urlbarStableCounter = 0;
  private navbarBottomScheduleController: AbortController | null = null;
  private browserStartupPromise: Promise<void> | null = null;
  private browserStartupCompleted = false;

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
    this.cancelNavbarBottomScheduling();
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
    if (this.isNavbarAtBottom) {
      return;
    }
    if (this.browserStartupCompleted) {
      this.executeNavbarToBottom();
      return;
    }
    this.scheduleNavbarToBottomMove();
  }

  private scheduleNavbarToBottomMove() {
    if (this.navbarBottomScheduleController) {
      return;
    }

    const controller = new AbortController();
    this.navbarBottomScheduleController = controller;

    this.waitForBrowserStartup()
      .then(() => {
        if (controller.signal.aborted) {
          return;
        }

        this.navbarBottomScheduleController = null;
        this.executeNavbarToBottom();
      })
      .catch((error: unknown) => {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error waiting for browser startup:`,
          error,
        );

        if (controller.signal.aborted) {
          return;
        }

        this.navbarBottomScheduleController = null;
        this.executeNavbarToBottom();
      });
  }

  private cancelNavbarBottomScheduling() {
    if (this.navbarBottomScheduleController) {
      this.navbarBottomScheduleController.abort();
      this.navbarBottomScheduleController = null;
    }
  }

  private removeUrlbarPopoverAttribute() {
    const urlbar = document?.getElementById("urlbar");
    if (!urlbar) {
      return;
    }

    if (!urlbar.hasAttribute("popover")) {
      return;
    }

    try {
      urlbar.removeAttribute("popover");
    } catch (error) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Failed to remove popover attribute from urlbar`,
        error,
      );
    }
  }

  private executeNavbarToBottom() {
    if (this.isNavbarAtBottom) {
      return;
    }
    if (config().uiCustomization.navbar.position !== "bottom") {
      return;
    }

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

    try {
      this.originalNavbarParent = navbar.parentElement;
      this.originalNavbarNextSibling = navbar.nextElementSibling;

      fullscreenWrapper.after(navbar);
      this.isNavbarAtBottom = true;

      this.removeUrlbarPopoverAttribute();

      // Delay urlbar fix to avoid early DOM mutation causing layout glitches (Issue #1936)
      this.scheduleFixUrlbarInputContainer();
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error in moveNavbarToBottom:`,
        error,
      );
    }
  }

  private moveNavbarToTop() {
    this.cancelNavbarBottomScheduling();
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

  /**
   * Schedule fix for urlbar input container with retries.
   * Sometimes Firefox builds urlbar sub-DOM asynchronously; executing too early causes
   * the address bar to render in the wrong place (Issue #1936).
   */
  private scheduleFixUrlbarInputContainer() {
    // Start after SessionStore initialization, then attempt several times with backoff.
    const sessionStore = globalThis.SessionStore;
    if (!sessionStore?.promiseInitialized) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} SessionStore not ready for urlbar fix`,
      );
      return;
    }

    sessionStore.promiseInitialized
      .then(() => {
        this.retryFixUrlbarInputContainer();
        this.startUrlbarPositionPolling();
        this.startUrlbarMutationObserver();
      })
      .catch((error: unknown) => {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error waiting for SessionStore initialization:`,
          error,
        );
      });
  }

  private retryFixUrlbarInputContainer(attempt = 0) {
    const MAX_ATTEMPTS = 10;
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );

    if (urlbarView && urlbarInputContainer) {
      // Only move if not already placed
      if (urlbarView.nextElementSibling !== urlbarInputContainer) {
        try {
          urlbarView.after(urlbarInputContainer);
          console.info(
            `${DOMLayoutManager.DEBUG_PREFIX} urlbar input container positioned (attempt ${attempt})`,
          );
        } catch (error: unknown) {
          console.error(
            `${DOMLayoutManager.DEBUG_PREFIX} Error moving urlbar input container:`,
            error,
          );
        }
      }
      return; // Success or already positioned
    }

    if (attempt < MAX_ATTEMPTS) {
      const delay = 50 + attempt * 50; // incremental backoff
      setTimeout(() => this.retryFixUrlbarInputContainer(attempt + 1), delay);
    } else {
      if (!urlbarView) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} urlbarView element not found after retries`,
        );
      }
      if (!urlbarInputContainer) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} urlbarInputContainer element not found after retries`,
        );
      }
    }
  }

  /** Start lightweight polling for a short period to ensure late DOM tweaks don't revert layout. */
  private startUrlbarPositionPolling() {
    // Clear existing
    if (this.urlbarFixIntervalId) {
      clearInterval(this.urlbarFixIntervalId);
      this.urlbarFixIntervalId = null;
    }

    const MAX_DURATION_MS = 7000; // stop after 7s
    const REQUIRED_STABLE_TICKS = 5; // number of consecutive OK ticks before stopping
    const INTERVAL_MS = 300;
    const perf = globalThis.performance;
    const start = perf ? perf.now() : Date.now();
    this.urlbarStableCounter = 0;

    this.urlbarFixIntervalId = setInterval(() => {
      const ok = this.ensureUrlbarOrder();
      if (ok) {
        this.urlbarStableCounter++;
      } else {
        this.urlbarStableCounter = 0; // reset stability window
      }

      const elapsed = (perf ? perf.now() : Date.now()) - start;
      if (
        this.urlbarStableCounter >= REQUIRED_STABLE_TICKS ||
        elapsed > MAX_DURATION_MS
      ) {
        if (this.urlbarFixIntervalId) {
          clearInterval(this.urlbarFixIntervalId);
          this.urlbarFixIntervalId = null;
          console.info(
            `${DOMLayoutManager.DEBUG_PREFIX} Stopped urlbar polling (stable=${
              this.urlbarStableCounter >= REQUIRED_STABLE_TICKS
            }, elapsed=${Math.round(elapsed)}ms)`,
          );
        }
      }
    }, INTERVAL_MS) as unknown as number; // casting due to TS DOM lib / Gecko types mismatch
  }

  /** Use MutationObserver for structural changes after initial stabilization (customization UI, extensions, etc.) */
  private startUrlbarMutationObserver() {
    // Disconnect existing observer
    this.urlbarMutationObserver?.disconnect();

    const target = document?.getElementById("nav-bar");
    if (!target) return;

    this.urlbarMutationObserver = new MutationObserver((mutations) => {
      // Debounced check: if any childList changes might affect order
      const needsCheck = mutations.some((m) => m.type === "childList");
      if (needsCheck) {
        // microtask -> macrotask to let DOM settle
        setTimeout(() => this.ensureUrlbarOrder(), 0);
      }
    });

    try {
      this.urlbarMutationObserver.observe(target, {
        childList: true,
        subtree: true,
      });
    } catch (e) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} Failed to start MutationObserver`,
        e,
      );
    }
  }

  /** Ensure the urlbar input container is placed after .urlbarView. Returns true if layout is correct. */
  private ensureUrlbarOrder(): boolean {
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );
    if (!urlbarView || !urlbarInputContainer) return false;
    if (urlbarView.nextElementSibling === urlbarInputContainer) return true;
    try {
      urlbarView.after(urlbarInputContainer);
      console.info(
        `${DOMLayoutManager.DEBUG_PREFIX} ensureUrlbarOrder: corrected ordering`,
      );
      return true;
    } catch (e) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} ensureUrlbarOrder failed`,
        e,
      );
      return false;
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

  private async waitForBrowserStartup(): Promise<void> {
    if (this.browserStartupCompleted) {
      return;
    }

    if (!this.browserStartupPromise) {
      this.browserStartupPromise = (async () => {
        try {
          await this.waitForDocumentLoad();

          const firefoxWindow = window as typeof window & {
            delayedStartupPromise?: Promise<unknown>;
            gBrowserInit?: {
              delayedStartupPromise?: Promise<unknown>;
              delayedStartupFinished?: boolean;
            };
          };

          const delayedPromise =
            firefoxWindow.delayedStartupPromise ??
            firefoxWindow.gBrowserInit?.delayedStartupPromise;

          if (delayedPromise && typeof delayedPromise.then === "function") {
            try {
              await delayedPromise;
              return;
            } catch (error) {
              console.warn(
                `${DOMLayoutManager.DEBUG_PREFIX} delayedStartupPromise rejected`,
                error,
              );
            }
          }

          await this.waitForDelayedStartup(firefoxWindow);
        } catch (error) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} waitForBrowserStartup failed`,
            error,
          );
        }
      })().finally(() => {
        this.browserStartupCompleted = true;
      });
    }

    await this.browserStartupPromise;
    this.removeUrlbarPopoverAttribute();
  }

  private waitForDocumentLoad(): Promise<void> {
    if (typeof window === "undefined" || document.readyState === "complete") {
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      window.addEventListener("load", () => resolve(), { once: true });
    });
  }

  private async waitForDelayedStartup(
    firefoxWindow: typeof window & {
      gBrowserInit?: { delayedStartupFinished?: boolean };
    },
  ): Promise<void> {
    const OBSERVER_TOPIC = "browser-delayed-startup-finished";
    const INTERVAL_MS = 50;
    const MAX_WAIT_MS = 20000;

    if (firefoxWindow.gBrowserInit?.delayedStartupFinished) {
      return;
    }

    const servicesContainer = globalThis as typeof globalThis & {
      Services?: {
        obs?: {
          addObserver?: (observer: unknown, topic: string) => void;
          removeObserver?: (observer: unknown, topic: string) => void;
        };
      };
    };

    await new Promise<void>((resolve) => {
      const perf = globalThis.performance;
      const start = perf?.now ? perf.now() : Date.now();
      let settled = false;
      let intervalId: number | null = null;
      const services = servicesContainer.Services;

      type Observer = {
        observe: (subject: unknown, topic: string, data?: string) => void;
      };

      let observer: Observer | null = null;

      const finish = () => {
        if (settled) {
          return;
        }
        settled = true;

        if (intervalId !== null) {
          window.clearInterval(intervalId);
          intervalId = null;
        }

        if (observer && services?.obs?.removeObserver) {
          try {
            services.obs.removeObserver(observer, OBSERVER_TOPIC);
          } catch (error) {
            console.warn(
              `${DOMLayoutManager.DEBUG_PREFIX} Failed to remove delayed-startup observer`,
              error,
            );
          }
        }

        resolve();
      };

      if (services?.obs?.addObserver && services.obs.removeObserver) {
        observer = {
          observe: (_subject, topic) => {
            if (topic === OBSERVER_TOPIC) {
              finish();
            }
          },
        };

        try {
          services.obs.addObserver(observer, OBSERVER_TOPIC);
        } catch (error) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} Failed to add delayed-startup observer`,
            error,
          );
          observer = null;
        }
      }

      const checkReady = () => {
        if (firefoxWindow.gBrowserInit?.delayedStartupFinished) {
          finish();
          return;
        }

        const now = perf?.now ? perf.now() : Date.now();
        if (now - start >= MAX_WAIT_MS) {
          finish();
        }
      };

      intervalId = window.setInterval(checkReady, INTERVAL_MS);
      checkReady();
    });
  }
}

