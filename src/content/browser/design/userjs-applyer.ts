/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const currentUserJS = Services.prefs.getCharPref("browser.userjs.location", "");

export async function applyUserJS(path: string) {
  const userjs = await (await fetch(path)).text();
  const p_userjs = userjs.replaceAll(/\/\/[\/]*.*\n/g, "\n");
  for (const line of p_userjs.split("\n")) {
    if (line.includes("user_pref")) {
      const tmp = line.replaceAll("user_pref(", "").replaceAll(");", "");
      let [prefName, value, ..._] = tmp.split(",");
      prefName = prefName.trim().replaceAll('"', "");
      value = value.trim();

      if (value === "true" || value === "false") {
        //console.log(prefName);
        Services.prefs
          .getDefaultBranch("")
          .setBoolPref(prefName, value === "true");
      } else if (value.includes('"')) {
        Services.prefs
          .getDefaultBranch("")
          .setStringPref(prefName, value.replace('"', ""));
      } else if (!Number.isNaN(value)) {
        // integer
        Services.prefs
          .getDefaultBranch("")
          .setIntPref(prefName, value as unknown as number);
      }
    }
  }
  Services.prefs.setStringPref("browser.userjs.location", path);
}

export async function resetPreferencesWithUserJsContents(path: string) {
  const text = await (await fetch(path)).text();
  const prefPattern = /user_pref\("([^"]+)",\s*(true|false|\d+|"[^"]*")\);/g;

  const match = prefPattern.exec(text);
  while (match !== null) {
    if (!match[0].startsWith("//")) {
      const settingName = match[1];
      await new Promise((resolve) => {
        setTimeout(() => {
          Services.prefs.clearUserPref(settingName);
          console.info(`resetting ${settingName}`);
        }, 100);
        resolve(undefined);
      });
    }
  }
}
