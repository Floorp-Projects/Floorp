/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRStartPageParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRStartPage:GetCurrentTopSites": {
        // Debug: Log that we received the message to get current top sites
        console.debug(
          "[NRStartPageParent] Received message: NRStartPage:GetCurrentTopSites",
        );

        const { NewTabUtils } = ChromeUtils.importESModule(
          "resource://gre/modules/NewTabUtils.sys.mjs",
        );
        console.debug("[NRStartPageParent] Imported NewTabUtils");

        const { AboutNewTab } = ChromeUtils.importESModule(
          "resource:///modules/AboutNewTab.sys.mjs",
        );
        console.debug("[NRStartPageParent] Imported AboutNewTab");

        let topSites = [];
        try {
          const aboutNewTabSites = AboutNewTab.getTopSites();
          console.debug(
            "[NRStartPageParent] AboutNewTab.getTopSites() result:",
            aboutNewTabSites,
          );

          if (aboutNewTabSites.length > 0) {
            topSites = aboutNewTabSites;
            console.debug(
              "[NRStartPageParent] Using AboutNewTab.getTopSites()",
            );
          } else {
            topSites = await NewTabUtils.activityStreamLinks.getTopSites();
            console.debug(
              "[NRStartPageParent] Using NewTabUtils.activityStreamLinks.getTopSites()",
              topSites,
            );
          }
        } catch (e) {
          console.error(
            "[NRStartPageParent] Error while getting top sites:",
            e,
          );
        }

        this.sendAsyncMessage(
          "NRStartPage:GetCurrentTopSites",
          JSON.stringify({
            topsites: topSites,
          }),
        );
        console.debug(
          "[NRStartPageParent] Sent async message with top sites:",
          topSites,
        );
        break;
      }

      case "NRStartPage:GetFolderPathFromDialog": {
        const folderPicker = Cc["@mozilla.org/filepicker;1"].createInstance(
          Ci.nsIFilePicker,
        );
        const mode = Ci.nsIFilePicker.modeGetFolder;

        folderPicker.init(
          this.browsingContext,
          "Floorp",
          mode,
        );
        folderPicker.appendFilters(Ci.nsIFilePicker.filterAll);
        const result = await new Promise((resolve) =>
          folderPicker.open(resolve)
        );

        if (result == folderPicker.returnOK) {
          const file = folderPicker.file;
          const path = file.path;
          this.sendAsyncMessage(
            "NRStartPage:GetFolderPathFromDialog",
            JSON.stringify({
              success: true,
              path,
            }),
          );
        } else {
          this.sendAsyncMessage(
            "NRStartPage:GetFolderPathFromDialog",
            JSON.stringify({
              success: false,
            }),
          );
        }
        break;
      }

      case "NRStartPage:GetRandomImageFromFolder": {
        const { folderPath } = message.data;
        const directory = Cc["@mozilla.org/file/local;1"].createInstance(
          Ci.nsIFile,
        );
        directory.initWithPath(folderPath.toString());

        if (!directory.exists() || !directory.isDirectory()) {
          this.sendAsyncMessage(
            "NRStartPage:GetRandomImageFromFolder",
            JSON.stringify({
              success: false,
              image: null,
            }),
          );
          break;
        }

        const imageFiles: nsIFile[] = [];
        const entries = directory.directoryEntries;
        const imageExtensions = [
          ".jpg",
          ".jpeg",
          ".png",
          ".gif",
          ".webp",
          ".avif",
          ".bmp",
        ];

        while (entries.hasMoreElements()) {
          const file = entries.getNext().QueryInterface(Ci.nsIFile);
          const fileName = file.leafName.toLowerCase();

          if (imageExtensions.some((ext) => fileName.endsWith(ext))) {
            imageFiles.push(file);
          }
        }

        if (imageFiles.length === 0) {
          this.sendAsyncMessage(
            "NRStartPage:GetRandomImageFromFolder",
            JSON.stringify({
              success: false,
              image: null,
            }),
          );
          break;
        }

        const randomIndex = Math.floor(Math.random() * imageFiles.length);
        const randomFile = imageFiles[randomIndex];

        try {
          const dataUrl = this.fileToDataURL(randomFile);
          if (dataUrl) {
            this.sendAsyncMessage(
              "NRStartPage:GetRandomImageFromFolder",
              JSON.stringify({
                success: true,
                image: dataUrl,
                fileName: randomFile.leafName,
              }),
            );
          } else {
            this.sendAsyncMessage(
              "NRStartPage:GetRandomImageFromFolder",
              JSON.stringify({
                success: false,
                image: null,
              }),
            );
          }
        } catch (e) {
          console.error("Error converting file to data URL:", e);
          this.sendAsyncMessage(
            "NRStartPage:GetRandomImageFromFolder",
            JSON.stringify({
              success: false,
              image: null,
            }),
          );
        }
        break;
      }
    }
  }

  fileToDataURL(file: nsIFile): string | null {
    try {
      let mimeType = "";
      const fileName = file.leafName.toLowerCase();
      if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) {
        mimeType = "image/jpeg";
      } else if (fileName.endsWith(".png")) {
        mimeType = "image/png";
      } else if (fileName.endsWith(".gif")) {
        mimeType = "image/gif";
      } else if (fileName.endsWith(".webp")) {
        mimeType = "image/webp";
      } else if (fileName.endsWith(".avif")) {
        mimeType = "image/avif";
      } else if (fileName.endsWith(".bmp")) {
        mimeType = "image/bmp";
      } else {
        return null;
      }

      const fileInputStream = Cc["@mozilla.org/network/file-input-stream;1"]
        .createInstance(
          Ci.nsIFileInputStream,
        );
      fileInputStream.init(file, 0x01, 0, 0);

      const binaryInputStream = Cc["@mozilla.org/binaryinputstream;1"]
        .createInstance(
          Ci.nsIBinaryInputStream,
        );
      binaryInputStream.setInputStream(fileInputStream);

      const fileSize = file.fileSize;
      const data = binaryInputStream.readBytes(fileSize);

      const base64 = btoa(data);

      return `data:${mimeType};base64,${base64}`;
    } catch (e) {
      console.error("Error in fileToDataURL:", e);
      return null;
    }
  }
}
