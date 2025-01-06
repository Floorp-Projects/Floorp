/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "./type";

export class DataManager {
  private get ssbStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  constructor() {}

  public async getCurrentSsbData(): Promise<Record<string, Manifest>> {
    const fileExists = await IOUtils.exists(this.ssbStoreFile);
    if (!fileExists) {
      await IOUtils.writeJSON(this.ssbStoreFile, {});
      return {};
    }
    return await IOUtils.readJSON(this.ssbStoreFile);
  }

  private async overrideCurrentSsbData(ssbData: object) {
    await IOUtils.writeJSON(this.ssbStoreFile, ssbData);
  }

  public async saveSsbData(manifest: Manifest) {
    const start_url = manifest.start_url;
    const currentSsbData = await this.getCurrentSsbData();
    currentSsbData[start_url] = manifest;
    await this.overrideCurrentSsbData(currentSsbData);
  }

  public async removeSsbData(url: string) {
    const list = await this.getCurrentSsbData();
    if (list[url]) {
      delete list[url];
      await this.overrideCurrentSsbData(list);
    }
  }
}
