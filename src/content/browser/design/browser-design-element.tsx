/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO: This file will be removed in the future. We need to use ?url instead of @import in the CSS files.
const themeCSSs = {
  LeptonUI: `@import url(chrome://floorp/skin/designs/lepton/leptonChrome.css?${updateDateAndTime()});
               @import url(chrome://floorp/skin/designs/lepton/leptonContent.css?${updateDateAndTime()});`,
  FluerialUI: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI.css?${updateDateAndTime()});`,
  FluerialUIMultitab: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI.css?${updateDateAndTime()});
                         @import url(chrome://floorp/skin/designs/fluerialUI/fluerial-multitab.css);`,
  LeptonVerticalTabs: `@import url(chrome://floorp/skin/designs/lepton/leptonVerticalTabs.css?${updateDateAndTime()});`,
  FluerialVerticalTabs: `@import url(chrome://floorp/skin/designs/fluerialUI/fluerialUI-verticalTabs.css?${updateDateAndTime()});`,
};

function updateDateAndTime() {
  return new Date().getTime();
}
function multirowTabEnabled() {
  return Services.prefs.getIntPref("floorp.tabbar.style") === 1;
}
function verticalTabsEnabled() {
  return Services.prefs.getIntPref("floorp.browser.tabbar.settings") === 2;
}
function userInterface() {
  return Services.prefs.getIntPref("floorp.browser.user.interface", 0);
}

function getCurrentBrowserDesignStyles() {
  switch (userInterface()) {
    case 3:
      return themeCSSs.LeptonUI;
    case 8:
      if (multirowTabEnabled()) {
        return themeCSSs.FluerialUIMultitab;
      }
      if (verticalTabsEnabled()) {
        // If vertical tabs are enabled, Add the vertical tabs Injections.
        return `${themeCSSs.FluerialUI} ${themeCSSs.FluerialVerticalTabs}`;
      }
      return themeCSSs.FluerialUI;
    default:
      // Return empty string if no design is selected.
      // This means the Proton Firefox design is used.
      return "";
  }
}

export function BrowserDesignElement() {
  return <style id="browserdesign">{getCurrentBrowserDesignStyles()}</style>;
}
