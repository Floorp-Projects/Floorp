/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SiteSpecificBrowserManager } from "../ssbManager";
import { ImageTools } from "../imageTools";
import type { Manifest } from "../type";

export class WindowsSupport {
  private static shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
    Ci.nsIWindowsShellService
  );

  private static uiUtils = Cc["@mozilla.org/windows-ui-utils;1"].getService(
    Ci.nsIWindowsUIUtils
  );

  private static taskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
    Ci.nsIWinTaskbar
  );

  private static nsIFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath"
  );

  constructor(private ssbManager: SiteSpecificBrowserManager) {}

  private buildGroupId(id: string) {
    return `ablaze.floorp.ssb.${id}`;
  }

  async install(ssb: Manifest) {
    if (!this.ssbManager.useOSIntegration()) {
      return;
    }

    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    await IOUtils.makeDirectory(dir, {
      ignoreExisting: true,
    });

    let iconFile = new WindowsSupport.nsIFile(PathUtils.join(dir, "icon.ico"));

    // We should be embedding multiple icon sizes, but the current icon encoder
    // does not support this. For now just embed a sensible size.
    const icon = ssb.icon;
    if (icon) {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(icon)
      );
      ImageTools.saveIcon(container, 128, 128, iconFile);
    } else {
      iconFile = null;
    }

    WindowsSupport.shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      ssb.name,
      iconFile,
      0,
      this.buildGroupId(ssb.id),
      "Programs",
      `${ssb.name}.lnk`
    );
  }

  /**
   * @param {SiteSpecificBrowser} ssb the SSB to uninstall.
   */
  async uninstall(ssb: Manifest) {
    if (!this.ssbManager.useOSIntegration()) {
      return;
    }

    try {
      const startMenu = `${
        Services.dirsvc.get("Home", Ci.nsIFile).path
      }\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\`;
      await IOUtils.remove(`${startMenu + ssb.name}.lnk`);
    } catch (e) {
      console.error(e);
    }

    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    try {
      await IOUtils.remove(dir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Applies the necessary OS integration to an open SSB.
   *
   * Sets the window icon based on the available icons.
   *
   * @param {SiteSpecificBrowser} ssb the SSB.
   * @param {DOMWindow} aWindow the window showing the SSB.
   */
  async applyOSIntegration(ssb: Manifest, aWindow: Window) {
    WindowsSupport.taskbar.setGroupIdForWindow(
      aWindow,
      this.buildGroupId(ssb.id)
    );
    const getIcon = async (size: number) => {
      const icon = ssb.icon;
      if (!icon) {
        return null;
      }

      try {
        const image = await ImageTools.loadImage(Services.io.newURI(icon));
        return image.container;
      } catch (e) {
        console.error(e);
        return null;
      }
    };

    if (this.ssbManager.useOSIntegration()) {
      return;
    }

    const icons = await Promise.all([
      getIcon(WindowsSupport.uiUtils.systemSmallIconSize),
      getIcon(WindowsSupport.uiUtils.systemLargeIconSize),
    ]);

    if (icons[0] || icons[1]) {
      WindowsSupport.uiUtils.setWindowIcon(aWindow, icons[0], icons[1]);
    }
  }
}
