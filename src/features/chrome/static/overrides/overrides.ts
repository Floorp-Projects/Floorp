/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ovrride_modules = import.meta.glob("./modules/*/index.ts");
const modules = {
  override: {} as Record<string, () => Promise<unknown>>,
};

Object.entries(ovrride_modules).map((v) => {
  modules.override[v[0].replace("./modules/", "").replace("/index.ts", "")] =
    v[1] as () => Promise<unknown>;
});

export class Overrides {
  private static instance: Overrides;
  public static getInstance() {
    if (!Overrides.instance) {
      Overrides.instance = new Overrides();
    }
    return Overrides.instance;
  }

  constructor() {
    this.loadOverrides();
  }

  get loadedModules() {
    return JSON.parse(
      Services.prefs.getStringPref("noraneko.features.enabled", "{common: []}"),
    ).common as string[];
  }

  private loadOverrides() {
    for (const moduleName of this.loadedModules) {
      if (modules.override[moduleName]) {
        (async () => {
          (
            (await modules.override[moduleName]()) as {
              overrides?: (typeof Function)[];
            }
          ).overrides?.forEach((override) => override());
        })();
      }
    }
  }
}
