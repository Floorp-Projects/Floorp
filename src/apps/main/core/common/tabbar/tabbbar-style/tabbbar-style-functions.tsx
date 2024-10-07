/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { checkPaddingEnabled } from "./titilebar-padding";
import { config } from "../../designs/configs";
import { TabbarStyleModifyCSSElement } from "./tabbar-style-element";
import { render } from "@nora/solid-xul";

export namespace gTabbarStyleFunctions {
  function getPanelUIMenuButton(): XULElement | null {
    return document?.querySelector("#PanelUI-menu-button") as XULElement | null;
  }
  function getTabbarElement(): XULElement | null {
    return document?.querySelector("#TabsToolbar") as XULElement | null;
  }
  function getTitleBarElement(): XULElement | null {
    return document?.querySelector("#titlebar") as XULElement | null;
  }
  function getNavbarElement(): XULElement | null {
    return document?.querySelector("#nav-bar") as XULElement | null;
  }
  function getNavigatorToolboxtabbarElement(): XULElement | null {
    return document?.querySelector("#navigator-toolbox") as XULElement | null;
  }
  function getBrowserElement(): XULElement | null {
    return document?.querySelector("#browser") as XULElement | null;
  }
  function getUrlbarContainer(): XULElement | null {
    return document?.querySelector(".urlbar-container") as XULElement | null;
  }
  function isVerticalTabbar() {
    return config().tabbar.tabbarStyle === "vertical";
  }

  export function revertToDefaultStyle() {
    getTabbarElement()?.removeAttribute("floorp-tabbar-display-style");
    getTabbarElement()?.removeAttribute("hidden");
    getTabbarElement()?.appendChild(
      document?.querySelector("#floorp-tabbar-window-manage-container") as Node,
    );
    getTitleBarElement()?.appendChild(getTabbarElement() as Node);
    getNavigatorToolboxtabbarElement()?.prepend(getTitleBarElement() as Node);
    document?.querySelector("#floorp-tabbar-modify-css")?.remove();
    getTabbarElement()?.removeAttribute("floorp-tabbar-display-style");
    getUrlbarContainer()?.style.removeProperty("margin-top");
  }

  export function defaultTabbarStyle() {
    if (isVerticalTabbar()) {
      return;
    }

    getNavigatorToolboxtabbarElement()?.setAttribute(
      "floorp-tabbar-display-style",
      "0",
    );
  }

  export function hideHorizontalTabbar() {
    getTabbarElement()?.setAttribute("floorp-tabbar-display-style", "1");
  }

  export function optimiseToVerticalTabbar() {
    //optimize vertical tabbar
    getTabbarElement()?.setAttribute("hidden", "true");
    getNavbarElement()?.appendChild(
      document?.querySelector("#floorp-tabbar-window-manage-container") as Node,
    );
    checkPaddingEnabled();
  }

  export function bottomOfNavigationToolbar() {
    if (isVerticalTabbar()) {
      return;
    }
    getNavigatorToolboxtabbarElement()?.appendChild(getTabbarElement() as Node);
    getPanelUIMenuButton()?.after(
      document?.querySelector("#floorp-tabbar-window-manage-container") as Node,
    );
    getTabbarElement()?.setAttribute("floorp-tabbar-display-style", "2");
  }

  export function bottomOfWindow() {
    if (isVerticalTabbar()) {
      return;
    }

    getBrowserElement()?.after(getTitleBarElement() as Node);
    getPanelUIMenuButton()?.after(
      document?.querySelector("#floorp-tabbar-window-manage-container") as Node,
    );
    getTabbarElement()?.setAttribute("floorp-tabbar-display-style", "3");
    // set margin to the top of urlbar container & allow moving the window
    getUrlbarContainer()?.style.setProperty("margin-top", "5px");
  }

  export function applyTabbarStyle() {
    revertToDefaultStyle();
    render(
      () =>
        TabbarStyleModifyCSSElement({ style: config().tabbar.tabbarPosition }),
      document?.head,
      {
        hotCtx: import.meta.hot,
      },
    );

    switch (config().tabbar.tabbarPosition) {
      case "hide-horizontal-tabbar":
        hideHorizontalTabbar();
        break;
      case "optimise-to-vertical-tabbar":
        optimiseToVerticalTabbar();
        break;
      case "bottom-of-navigation-toolbar":
        bottomOfNavigationToolbar();
        break;
      case "bottom-of-window":
        bottomOfWindow();
        break;
      default:
        defaultTabbarStyle();
        break;
    }
  }
}
