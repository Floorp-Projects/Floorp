/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const currentUserJS = Services.prefs.getCharPref("browser.userjs.location", "");

export async function applyUserJS(path: string) {
  if (currentUserJS !== "") {
    await resetPreferencesWithUserJsContents(currentUserJS);
  }

  const userjs = await (await fetch(path)).text();
  const p_userjs = userjs.replaceAll(/^\s*\/\/.*$/gm, "");
  for (const line of p_userjs.split("\n")) {
    if (line.includes("user_pref")) {
      const tmp = line.replaceAll("user_pref(", "").replaceAll(");", "");
      let [prefName, value, ..._] = tmp.split(",");
      prefName = prefName.trim().replaceAll('"', "");
      value = value.trim();

      if (value === "true" || value === "false") {
        Services.prefs
          .getDefaultBranch("")
          .setBoolPref(prefName, value === "true");
      } else if (value.includes('"')) {
        Services.prefs
          .getDefaultBranch("")
          .setStringPref(prefName, value.replace(/"/g, ""));
      } else if (!Number.isNaN(Number(value))) {
        // integer
        Services.prefs.getDefaultBranch("").setIntPref(prefName, Number(value));
      }
    }
  }
  Services.prefs.setStringPref("browser.userjs.location", path);
}

export async function resetPreferencesWithUserJsContents(path: string) {
  const userjs = await (await fetch(path)).text();
  const p_userjs = userjs.replaceAll(/^\s*\/\/.*$/gm, "");
  for (const line of p_userjs.split("\n")) {
    if (line.includes("user_pref")) {
      const tmp = line.replaceAll("user_pref(", "").replaceAll(");", "");
      let [prefName] = tmp.split(",");
      prefName = prefName.trim().replaceAll('"', "");
      Services.prefs.clearUserPref(prefName);
    }
  }
  Services.prefs.setStringPref("browser.userjs.location", "");
}
