/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "./type";

export class DataManager {
  private static instance: DataManager;
  public static getInstance() {
    if (!DataManager.instance) {
      DataManager.instance = new DataManager();
    }
    return DataManager.instance;
  }

  private get ssbStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  public async getCurrentSsbData() {
    const fileExists = await IOUtils.exists(this.ssbStoreFile);
    if (!fileExists) {
      IOUtils.writeJSON(this.ssbStoreFile, {});
      return {};
    }
    return await IOUtils.readJSON(this.ssbStoreFile);
  }

  public async overrideCurrentSsbData(ssbData: object) {
    await IOUtils.writeJSON(this.ssbStoreFile, ssbData);
  }

  async saveSsbData(ssbData: Manifest) {
    console.log("saveSsbData", ssbData);
    const start_url = ssbData.start_url;
    const currentSsbData = await this.getCurrentSsbData();
    currentSsbData[start_url] = ssbData;
    await this.overrideCurrentSsbData(currentSsbData);
  }

  public async removeSsbData(id: string) {
    const list = await this.getCurrentSsbData();
    if (list[id]) {
      delete list[id];
      await this.overrideCurrentSsbData(list);
    }
  }
}
