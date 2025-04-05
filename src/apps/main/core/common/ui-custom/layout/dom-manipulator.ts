/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { config } from "@core/common/designs/configs";

export class DOMLayoutManager {
  private get personalToolbar(): Element {
    return document?.getElementById("PersonalToolbar") as Element;
  }

  private get navBar(): Element {
    return document?.getElementById("nav-bar") as Element;
  }

  private get fullscreenAndPointerlockWrapper(): Element {
    return document?.getElementById(
      "fullscreen-and-pointerlock-wrapper",
    ) as Element;
  }

  private get navigatorToolbox(): Element {
    return document?.getElementById("navigator-toolbox") as Element;
  }

  private get urlbarInputContainer(): Element {
    return document?.querySelector(".urlbar-input-container") as Element;
  }

  private get urlbarView(): Element {
    return document?.querySelector(".urlbarView") as Element;
  }

  setupDOMEffects() {
    this.setupNavbarPosition();
    this.setupBookmarksBarStatusMode();
  }

  private setupNavbarPosition() {
    createEffect(() => {
      if (config().uiCustomization.navbar.position === "bottom") {
        this.moveNavbarToBottom();
      } else {
        this.restoreNavbarPosition();
      }
    });
  }

  private setupBookmarksBarStatusMode() {
    createEffect(() => {
      if (config().uiCustomization.bookmarksBar.statusBarMode) {
        this.moveBookmarksBarToStatusBar();
        this.setupBookmarksBarVisibilityListener();
      } else {
        this.restoreBookmarksBarPosition();
      }
    });
  }

  private moveNavbarToBottom() {
    const navbar = this.navBar;
    const wrapper = this.fullscreenAndPointerlockWrapper;
    const urlbarInputContainer = this.urlbarInputContainer;
    const urlbarView = this.urlbarView;

    if (navbar && wrapper) {
      wrapper.after(navbar);
    }

    if (urlbarView && urlbarInputContainer) {
      urlbarView.after(urlbarInputContainer);
    }
  }

  private restoreNavbarPosition() {
    const navbar = this.navBar;
    const toolbox = this.navigatorToolbox;
    const urlbarInputContainer = this.urlbarInputContainer;
    const urlbarView = this.urlbarView;

    if (navbar && toolbox) {
      toolbox.appendChild(navbar);
    }

    if (urlbarView && urlbarInputContainer) {
      urlbarView.before(urlbarInputContainer);
    }

    if (!config().uiCustomization.bookmarksBar.statusBarMode) {
      const personalToolbar = this.personalToolbar;
      if (personalToolbar && toolbox) {
        toolbox.appendChild(personalToolbar);
      }
    }
  }

  private moveBookmarksBarToStatusBar() {
    const personalToolbar = this.personalToolbar;
    const wrapper = this.fullscreenAndPointerlockWrapper;

    if (personalToolbar && wrapper) {
      wrapper.after(personalToolbar);
    }
  }

  private restoreBookmarksBarPosition() {
    const personalToolbar = this.personalToolbar;
    const toolbox = this.navigatorToolbox;

    if (
      personalToolbar && toolbox &&
      config().uiCustomization.navbar.position !== "bottom"
    ) {
      toolbox.appendChild(personalToolbar);
    }

    document?.removeEventListener(
      "floorpOnLocationChangeEvent",
      this.handleLocationChange,
    );
  }

  private setupBookmarksBarVisibilityListener() {
    document?.addEventListener(
      "floorpOnLocationChangeEvent",
      this.handleLocationChange,
    );
  }

  private handleLocationChange = () => {
    if (!config().uiCustomization.bookmarksBar.statusBarMode) return;

    const personalToolbar = this.personalToolbar;
    if (!personalToolbar) return;

    try {
      const currentUrl = window.gFloorpOnLocationChange?.locationURI?.spec;
      const pref = Services.prefs.getStringPref(
        "browser.toolbars.bookmarks.visibility",
        "always",
      );

      if (
        (currentUrl === "about:newtab" || currentUrl === "about:home") &&
        pref === "newtab"
      ) {
        personalToolbar.setAttribute("collapsed", "false");
      } else if (pref === "always") {
        personalToolbar.setAttribute("collapsed", "false");
      } else {
        personalToolbar.setAttribute("collapsed", "true");
      }
    } catch (e) {
      console.error("Error in handleLocationChange:", e);
    }
  };
}
