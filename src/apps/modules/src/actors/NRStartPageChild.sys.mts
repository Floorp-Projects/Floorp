/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRStartPageChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRStartPageChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5186" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRStartPage 5186 ! or Chrome Page!");
      Cu.exportFunction(this.GetCurrentTopSites.bind(this), window, {
        defineAs: "NRGetCurrentTopSites",
      });
      Cu.exportFunction(this.GetFolderPathFromDialog.bind(this), window, {
        defineAs: "NRGetFolderPathFromDialog",
      });
      Cu.exportFunction(this.GetRandomImageFromFolder.bind(this), window, {
        defineAs: "NRGetRandomImageFromFolder",
      });
      Cu.exportFunction(this.FocusUrlBar.bind(this), window, {
        defineAs: "NRFocusUrlBar",
      });
    }
  }

  GetCurrentTopSites(callback: (topSites: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetCurrentTopSites = resolve;
    });
    this.sendAsyncMessage("NRStartPage:GetCurrentTopSites");
    promise.then((topSites) => callback(topSites));
  }

  resolveGetCurrentTopSites: ((topSites: string) => void) | null = null;

  GetFolderPathFromDialog(callback: (folderPath: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetFolderPathFromDialog = resolve;
    });
    this.sendAsyncMessage("NRStartPage:GetFolderPathFromDialog");
    promise.then((folderPath) => callback(folderPath));
  }

  resolveGetFolderPathFromDialog: ((folderPath: string) => void) | null = null;

  GetRandomImageFromFolder(
    folderPath: string,
    callback: (image: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetRandomImageFromFolder = resolve;
    });
    this.sendAsyncMessage("NRStartPage:GetRandomImageFromFolder", {
      folderPath,
    });
    promise.then((image) => callback(image));
  }

  resolveGetRandomImageFromFolder: ((image: string) => void) | null = null;

  FocusUrlBar() {
    this.sendAsyncMessage("NRStartPage:FocusUrlBar");
  }

  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRStartPage:GetCurrentTopSites": {
        this.resolveGetCurrentTopSites?.(message.data);
        this.resolveGetCurrentTopSites = null;
        break;
      }
      case "NRStartPage:GetFolderPathFromDialog": {
        this.resolveGetFolderPathFromDialog?.(message.data);
        this.resolveGetFolderPathFromDialog = null;
        break;
      }
      case "NRStartPage:GetRandomImageFromFolder": {
        this.resolveGetRandomImageFromFolder?.(message.data);
        this.resolveGetRandomImageFromFolder = null;
        break;
      }
    }
  }
}
