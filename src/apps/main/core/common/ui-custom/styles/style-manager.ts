/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { config } from "@core/common/designs/configs.ts";

import navbarBottomCSS from "./css/options/navbar-botttom.css?inline";
import movePageInsideSearchbarCSS from "./css/options/move_page_inside_searchbar.css?inline";
import treestyletabCSS from "./css/options/treestyletab.css?inline";
import msbuttonCSS from "./css/options/msbutton.css?inline";
import disableFullScreenNotificationCSS from "./css/options/disableFullScreenNotification.css?inline";
import deleteBorderCSS from "./css/options/delete-border.css?inline";
import stgLikeFloorpWorkspacesCSS from "./css/options/STG-like-floorp-workspaces.css?inline";
import multirowTabShowNewtabInTabbarCSS from "./css/options/multirowtab-show-newtab-button-in-tabbar.css?inline";
import multirowTabShowNewtabAtEndCSS from "./css/options/multirowtab-show-newtab-button-at-end.css?inline";
import bookmarkbarFocusExpandCSS from "./css/options/bookmarkbar_focus_expand.css?inline";

export class StyleManager {
  private styleElements: Map<string, HTMLStyleElement> = new Map();
  private pendingStartupStyles: Map<string, AbortController> = new Map();
  private browserStartupPromise: Promise<void> | null = null;
  private browserStartupCompleted = false;

  setupStyleEffects() {
    this.setupNavbarEffects();
    this.setupSearchbarEffects();
    this.setupDisplayEffects();
    this.setupSpecialEffects();
    this.setupMultirowTabEffects();
    this.setupBookmarkBarEffects();
  }

  private setupNavbarEffects() {
    createEffect(() => {
      const shouldPlaceAtBottom =
        config().uiCustomization.navbar.position === "bottom";

      this.applyStyle(
        "floorp-navvarcss",
        navbarBottomCSS,
        shouldPlaceAtBottom,
        {
          waitForStartup: true,
          onActivate: () => this.setNavbarBottomAttribute(true),
          onDeactivate: () => this.setNavbarBottomAttribute(false),
        },
      );
    });
  }

  private setupSearchbarEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-searchbartop",
        movePageInsideSearchbarCSS,
        config().uiCustomization.navbar.searchBarTop,
      );
    });
  }

  private setupDisplayEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-DFSN",
        disableFullScreenNotificationCSS,
        config().uiCustomization.display.disableFullscreenNotification,
      );

      this.applyStyle(
        "floorp-DB",
        deleteBorderCSS,
        config().uiCustomization.display.deleteBrowserBorder,
      );

      if (config().uiCustomization.display.hideUnifiedExtensionsButton) {
        this.createInlineStyle(
          "floorp-hide-unified-extensions-button",
          "#unified-extensions-button {display: none !important;}",
        );
      } else {
        this.removeStyle("floorp-hide-unified-extensions-button");
      }
    });
  }

  private setupSpecialEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-optimizefortreestyletab",
        treestyletabCSS,
        config().uiCustomization.special.optimizeForTreeStyleTab,
      );

      this.applyStyle(
        "floorp-hideForwardBackwardButton",
        msbuttonCSS,
        config().uiCustomization.special.hideForwardBackwardButton,
      );

      this.applyStyle(
        "floorp-STG-like-floorp-workspaces",
        stgLikeFloorpWorkspacesCSS,
        config().uiCustomization.special.stgLikeWorkspaces,
      );
    });
  }

  private setupMultirowTabEffects() {
    createEffect(() => {
      const isMultirowStyle = config().tabbar.tabbarStyle === "multirow";
      const newtabInsideEnabled =
        config().uiCustomization.multirowTab.newtabInsideEnabled;

      this.applyStyle(
        "floorp-newtabbuttoninmultirowtabbbar",
        multirowTabShowNewtabInTabbarCSS,
        newtabInsideEnabled && isMultirowStyle,
      );

      this.applyStyle(
        "floorp-newtabbuttonatendofmultirowtabbar",
        multirowTabShowNewtabAtEndCSS,
        !newtabInsideEnabled && isMultirowStyle,
      );
    });
  }

  private setupBookmarkBarEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-bookmarkbar-focus-expand",
        bookmarkbarFocusExpandCSS,
        config().uiCustomization.bookmarkBar?.focusExpand ?? false,
      );
    });
  }

  applyStyle(
    id: string,
    cssContent: string,
    condition: boolean,
    options?: {
      waitForStartup?: boolean;
      onActivate?: () => void;
      onDeactivate?: () => void;
    },
  ) {
    if (!condition) {
      this.cancelPendingStartup(id);
      options?.onDeactivate?.();
      this.removeStyle(id);
      return;
    }

    this.cancelPendingStartup(id);

    if (options?.waitForStartup) {
      this.scheduleStyleAfterStartup(id, cssContent, options.onActivate);
      return;
    }

    this.createStyle(id, cssContent);
    options?.onActivate?.();
  }

  private scheduleStyleAfterStartup(
    id: string,
    cssContent: string,
    onActivate?: () => void,
  ) {
    const controller = new AbortController();
    this.pendingStartupStyles.set(id, controller);

    this.waitForBrowserStartup()
      .then(() => {
        if (controller.signal.aborted) {
          return;
        }

        this.pendingStartupStyles.delete(id);
        this.createStyle(id, cssContent);
        onActivate?.();
      })
      .catch((error) => {
        console.error(
          `Failed to wait for browser startup before applying style: ${id}`,
          error,
        );

        if (controller.signal.aborted) {
          return;
        }

        this.pendingStartupStyles.delete(id);
        this.createStyle(id, cssContent);
        onActivate?.();
      });
  }

  private cancelPendingStartup(id: string) {
    const controller = this.pendingStartupStyles.get(id);
    if (!controller) {
      return;
    }

    controller.abort();
    this.pendingStartupStyles.delete(id);
  }

  private async waitForBrowserStartup(): Promise<void> {
    if (this.browserStartupCompleted) {
      return;
    }

    if (!this.browserStartupPromise) {
      this.browserStartupPromise = (async () => {
        try {
          await this.resolveBrowserStartup();
        } catch (error) {
          console.warn(
            "Wait for browser startup failed, applying styles immediately",
            error,
          );
        }
      })().finally(() => {
        this.browserStartupCompleted = true;
      });
    }

    await this.browserStartupPromise;
  }

  private async resolveBrowserStartup(): Promise<void> {
    if (typeof window === "undefined") {
      return;
    }

    await this.waitForDocumentLoad();

    const firefoxWindow = window as typeof window & {
      delayedStartupPromise?: Promise<unknown>;
      gBrowserInit?: {
        delayedStartupPromise?: Promise<unknown>;
        delayedStartupFinished?: boolean;
      };
    };

    const delayedPromise = firefoxWindow.delayedStartupPromise ??
      firefoxWindow.gBrowserInit?.delayedStartupPromise;

    if (delayedPromise && typeof delayedPromise.then === "function") {
      try {
        await delayedPromise;
        return;
      } catch (error) {
        console.warn("delayedStartupPromise rejected", error);
      }
    }

    await this.waitForDelayedStartup(firefoxWindow);
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

    const globalWithServices = globalThis as typeof globalThis & {
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
      const services = globalWithServices.Services;
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
            console.warn("Failed to remove delayed-startup observer", error);
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
          console.warn("Failed to add delayed-startup observer", error);
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

  private setNavbarBottomAttribute(enabled: boolean) {
    const root = document?.documentElement;
    if (!root) {
      return;
    }

    if (enabled) {
      root.setAttribute("data-floorp-navbar-bottom-ready", "true");
    } else {
      root.removeAttribute("data-floorp-navbar-bottom-ready");
    }
  }

  private createStyle(id: string, cssContent: string) {
    // Remove existing style before creating new one
    this.removeStyle(id);

    if (!document || !document.head) {
      console.warn("Document or document.head not available");
      return;
    }

    try {
      const styleTag = document.createElement("style");
      styleTag.setAttribute("id", id);
      styleTag.textContent = cssContent;
      document.head.appendChild(styleTag);
      this.styleElements.set(id, styleTag);
    } catch (error) {
      console.error(`Failed to create style with id: ${id}`, error);
    }
  }

  private createInlineStyle(id: string, cssContent: string) {
    // Use the same createStyle method since they do the same thing
    this.createStyle(id, cssContent);
  }

  private removeStyle(id: string) {
    this.cancelPendingStartup(id);

    if (!document) {
      console.warn("Document not available");
      return;
    }

    try {
      // Remove from DOM
      const existingElement = document.getElementById(id);
      if (existingElement) {
        existingElement.remove();
      }

      // Remove from internal map
      this.styleElements.delete(id);
    } catch (error) {
      console.error(`Failed to remove style with id: ${id}`, error);
    }
  }
}
