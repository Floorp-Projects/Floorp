/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ThumbnailsChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "PageThumbUtils",
                               "resource://gre/modules/PageThumbUtils.jsm");

class ThumbnailsChild extends ActorChild {
  receiveMessage(message) {
    if (message.name == "Browser:Thumbnail:Request") {
      /**
       * Remote thumbnail request handler for PageThumbs thumbnails.
       */
      let snapshot;
      let args = message.data.additionalArgs;
      let fullScale = args ? args.fullScale : false;
      if (fullScale) {
        snapshot = PageThumbUtils.createSnapshotThumbnail(this.content, null, args);
      } else {
        let snapshotWidth = message.data.canvasWidth;
        let snapshotHeight = message.data.canvasHeight;
        snapshot =
          PageThumbUtils.createCanvas(this.content, snapshotWidth, snapshotHeight);
        PageThumbUtils.createSnapshotThumbnail(this.content, snapshot, args);
      }

      snapshot.toBlob((aBlob) => {
        this.mm.sendAsyncMessage("Browser:Thumbnail:Response", {
          thumbnail: aBlob,
          id: message.data.id
        });
      });
    } else if (message.name == "Browser:Thumbnail:CheckState") {
      /**
       * Remote isSafeForCapture request handler for PageThumbs.
       */
      Services.tm.idleDispatchToMainThread(() => {
        let result = PageThumbUtils.shouldStoreContentThumbnail(this.content, this.mm.docShell);
        this.mm.sendAsyncMessage("Browser:Thumbnail:CheckState:Response", {
          result
        });
      });
    } else if (message.name == "Browser:Thumbnail:GetOriginalURL") {
      /**
       * Remote GetOriginalURL request handler for PageThumbs.
       */
      let channel = this.mm.docShell.currentDocumentChannel;
      let channelError = PageThumbUtils.isChannelErrorResponse(channel);
      let originalURL;
      try {
        originalURL = channel.originalURI.spec;
      } catch (ex) {}
      this.mm.sendAsyncMessage("Browser:Thumbnail:GetOriginalURL:Response", {
        channelError,
        originalURL,
      });
    }
  }
}
