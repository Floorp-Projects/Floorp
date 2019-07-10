/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "imgTools",
  "@mozilla.org/image/tools;1",
  "imgITools"
);

const Transferable = Components.Constructor(
  "@mozilla.org/widget/transferable;1",
  "nsITransferable"
);

this.clipboard = class extends ExtensionAPI {
  getAPI(context) {
    return {
      clipboard: {
        async setImageData(imageData, imageType) {
          if (AppConstants.platform == "android") {
            return Promise.reject({
              message:
                "Writing images to the clipboard is not supported on Android",
            });
          }
          let img;
          try {
            img = imgTools.decodeImageFromArrayBuffer(
              imageData,
              `image/${imageType}`
            );
          } catch (e) {
            return Promise.reject({
              message: `Data is not a valid ${imageType} image`,
            });
          }

          // Other applications can only access the copied image once the data
          // is exported via the platform-specific clipboard APIs:
          // nsClipboard::SelectionGetEvent (widget/gtk/nsClipboard.cpp)
          // nsClipboard::PasteDictFromTransferable (widget/cocoa/nsClipboard.mm)
          // nsDataObj::GetDib (widget/windows/nsDataObj.cpp)
          //
          // The common protocol for exporting a nsITransferable as an image is:
          // - Use nsITransferable::GetTransferData to fetch the stored data.
          // - QI imgIContainer on the pointer.
          // - Convert the image to the native clipboard format.
          //
          // Below we create a nsITransferable in the above format.
          let transferable = new Transferable();
          transferable.init(null);
          const kNativeImageMime = "application/x-moz-nativeimage";
          transferable.addDataFlavor(kNativeImageMime);

          // Internal consumers expect the image data to be stored as a
          // nsIInputStream. On Linux and Windows, pasted data is directly
          // retrieved from the system's native clipboard, and made available
          // as a nsIInputStream.
          //
          // On macOS, nsClipboard::GetNativeClipboardData (nsClipboard.mm) uses
          // a cached copy of nsITransferable if available, e.g. when the copy
          // was initiated by the same browser instance. To make sure that a
          // nsIInputStream is returned instead of the cached imgIContainer,
          // the image is exported as as `kNativeImageMime`. Data associated
          // with this type is converted to a platform-specific image format
          // when written to the clipboard. The type is not used when images
          // are read from the clipboard (on all platforms, not just macOS).
          // This forces nsClipboard::GetNativeClipboardData to fall back to
          // the native clipboard, and return the image as a nsITransferable.

          // The length should not be zero. (Bug 1493292)
          transferable.setTransferData(kNativeImageMime, img, 1);

          Services.clipboard.setData(
            transferable,
            null,
            Services.clipboard.kGlobalClipboard
          );
        },
      },
    };
  }
};
