/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { insert } from "@solid-xul/solid-xul";
import { BrowserStyleElement } from "./browser-style-element";

// initialize lepton
import("./lepton-config");

export class gFloorpDesignClass {
  private static get updateDateAndTime() {
    return new Date().getTime();
  }
  private static get multirowTabEnabled() {
    return Services.prefs.getIntPref("floorp.tabbar.style") === 1;
  }
  private static get verticalTabsEnabled() {
    return Services.prefs.getIntPref("floorp.browser.tabbar.settings") === 2;
  }
  private static get userInterface() {
    return Services.prefs.getIntPref("floorp.browser.user.interface", 0);
  }

  private static get getBrowserDesignElement() {
    return document.querySelector("#browserdesign");
  }

  private readonly prefs = [
    "floorp.browser.user.interface",
    "floorp.fluerial.roundVerticalTabs",
  ];

  private static get themeCSSs(): Record<string, string> {
    return {
      LeptonUI: `@import url(chrome://floorp/skin/designs/lepton/leptonChrome.css?${gFloorpDesignClass.updateDateAndTime});
                 @import url(chrome://floorp/skin/designs/lepton/leptonContent.css?${gFloorpDesignClass.updateDateAndTime});`,
      FluerialUI: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI.css?${gFloorpDesignClass.updateDateAndTime});`,
      FluerialUIMultitab: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI.css?${gFloorpDesignClass.updateDateAndTime});
                           @import url(chrome://floorp/skin/designs/fluerialUI/fluerial-multitab.css);`,
      LeptonVerticalTabs: `@import url(chrome://floorp/skin/designs/lepton/leptonVerticalTabs.css?${gFloorpDesignClass.updateDateAndTime});`,
      FluerialVerticalTabs: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI-verticalTabs.css?${gFloorpDesignClass.updateDateAndTime});`,
    };
  }

  private static instance: gFloorpDesignClass;
  public static getInstance() {
    if (!gFloorpDesignClass.instance) {
      gFloorpDesignClass.instance = new gFloorpDesignClass();
    }
    return gFloorpDesignClass.instance;
  }

  constructor() {
    gFloorpDesignClass.setBrowserDesign();
    for (const pref of this.prefs) {
      Services.prefs.addObserver(pref, gFloorpDesignClass.setBrowserDesign);
    }

    Services.obs.addObserver(
      gFloorpDesignClass.setBrowserDesign,
      "update-photon-pref",
    );

    window.gURLBar._updateLayoutBreakoutDimensions();
    insert(
      document.head,
      <BrowserStyleElement />,
      document.head?.lastElementChild,
    );

    console.log("gFloorpDesignClass initialized");
  }

  public static setBrowserDesign() {
    if (gFloorpDesignClass.getBrowserDesignElement) {
      gFloorpDesignClass.getBrowserDesignElement.remove();
    }

    const tag = document.createElement("style");
    tag.setAttribute("id", "browserdesign");

    switch (gFloorpDesignClass.userInterface) {
      case 3:
        if (gFloorpDesignClass.verticalTabsEnabled) {
          tag.innerText = `${gFloorpDesignClass.themeCSSs.LeptonUI} ${gFloorpDesignClass.themeCSSs.LeptonVerticalTabs}`;
        } else {
          tag.innerText = gFloorpDesignClass.themeCSSs.LeptonUI;
        }
        break;
      case 8:
        if (gFloorpDesignClass.multirowTabEnabled) {
          tag.innerText = gFloorpDesignClass.themeCSSs.FluerialUIMultitab;
        } else if (gFloorpDesignClass.verticalTabsEnabled) {
          tag.innerText = `${gFloorpDesignClass.themeCSSs.FluerialUI} ${gFloorpDesignClass.themeCSSs.FluerialVerticalTabs}`;
        } else {
          tag.innerText = gFloorpDesignClass.themeCSSs.FluerialUI;
        }
        break;
    }

    document.head?.appendChild(tag);

    setTimeout(() => {
      window.gURLBar._updateLayoutBreakoutDimensions();
    }, 100);

    setTimeout(() => {
      window.gURLBar._updateLayoutBreakoutDimensions();
    }, 500);
  }
}
