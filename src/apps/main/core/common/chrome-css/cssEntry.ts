/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);

/**
 * CSS Entry class for loading and managing CSS files
 */
export class CSSEntry {
  path: string;
  leafName: string;
  lastModifiedTime: number;
  SHEET: number;
  _enabled = false;
  flag?: boolean;

  private sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
    Ci.nsIStyleSheetService,
  );

  constructor(aFile: string, folder: string) {
    // Use PathUtils.join for OS-independent path handling
    this.path = PathUtils.join(folder, aFile);
    this.leafName = aFile;
    this.lastModifiedTime = 1;

    if (/^xul-|\.as\.css$/i.test(this.leafName)) {
      this.SHEET = Ci.nsIStyleSheetService.AGENT_SHEET;
    } else if (/\.author\.css$/i.test(this.leafName)) {
      this.SHEET = Ci.nsIStyleSheetService.AUTHOR_SHEET;
    } else {
      this.SHEET = Ci.nsIStyleSheetService.USER_SHEET;
    }
  }

  get enabled(): boolean {
    return this._enabled;
  }

  set enabled(isEnable: boolean) {
    this._enabled = isEnable;

    try {
      const fileObj = new FileUtils.File(this.path);
      const uri = Services.io.newFileURI(fileObj);

      IOUtils.exists(this.path)
        .then((exists) => {
          if (exists && isEnable) {
            if (this.sss.sheetRegistered(uri, this.SHEET)) {
              IOUtils.stat(this.path).then((stat) => {
                if (this.lastModifiedTime !== stat.lastModified) {
                  this.sss.unregisterSheet(uri, this.SHEET);
                  this.sss.loadAndRegisterSheet(uri, this.SHEET);
                  this.lastModifiedTime = Number(stat.lastModified);
                }
              });
            } else {
              this.sss.loadAndRegisterSheet(uri, this.SHEET);
            }
          } else {
            this.sss.unregisterSheet(uri, this.SHEET);
          }
        })
        .catch((error) => {
          console.error("Error checking CSS file:", error);
        });
    } catch (error) {
      console.error("Error setting CSS enabled state:", error);
    }
  }
}
