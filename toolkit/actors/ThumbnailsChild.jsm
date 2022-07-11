/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ThumbnailsChild"];

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm"
);

class ThumbnailsChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "Browser:Thumbnail:ContentInfo": {
        let [width, height] = lazy.PageThumbUtils.getContentSize(
          this.contentWindow
        );
        return { width, height };
      }
      case "Browser:Thumbnail:CheckState": {
        /**
         * Remote isSafeForCapture request handler for PageThumbs.
         */
        return new Promise(resolve =>
          Services.tm.idleDispatchToMainThread(() => {
            if (!this.manager) {
              // If we have no manager, our actor has been destroyed, which
              // means we can't respond, and trying to touch
              // `this.contentWindow` or `this.browsingContext` will throw.
              // The `sendQuery` call in the parent will already have been
              // rejected when the actor was destroyed, so there's no need to
              // reject our promise or log an additional error.
              return;
            }

            let result = lazy.PageThumbUtils.shouldStoreContentThumbnail(
              this.contentWindow,
              this.browsingContext.docShell
            );
            resolve(result);
          })
        );
      }
      case "Browser:Thumbnail:GetOriginalURL": {
        /**
         * Remote GetOriginalURL request handler for PageThumbs.
         */
        let channel = this.browsingContext.docShell.currentDocumentChannel;
        let channelError = lazy.PageThumbUtils.isChannelErrorResponse(channel);
        let originalURL;
        try {
          originalURL = channel.originalURI.spec;
        } catch (ex) {}
        return { channelError, originalURL };
      }
    }
    return undefined;
  }
}
