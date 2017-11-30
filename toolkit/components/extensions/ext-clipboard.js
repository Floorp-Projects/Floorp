/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "imgTools",
                                   "@mozilla.org/image/tools;1", "imgITools");

const SupportsInterfacePointer = Components.Constructor(
    "@mozilla.org/supports-interface-pointer;1", "nsISupportsInterfacePointer");
const Transferable = Components.Constructor(
    "@mozilla.org/widget/transferable;1", "nsITransferable");

this.clipboard = class extends ExtensionAPI {
  getAPI(context) {
    return {
      clipboard: {
        async setImageData(imageData, imageType) {
          if (AppConstants.platform == "android") {
            return Promise.reject({message: "Writing images to the clipboard is not supported on Android"});
          }
          let mimeType = `image/${imageType}`;
          let container;
          try {
            container = imgTools.decodeImageFromArrayBuffer(imageData, mimeType);
          } catch (e) {
            return Promise.reject({message: `Data is not a valid ${imageType} image`});
          }

          // Other applications can only access the copied image once the data
          // is exported via the platform-specific clipboard APIs:
          // nsClipboard::SelectionGetEvent (widget/gtk/nsClipboard.cpp)
          // nsClipboard::PasteDictFromTransferable (widget/cocoa/nsClipboard.mm)
          // nsDataObj::GetDib (widget/windows/nsDataObj.cpp)
          //
          // The common protocol for exporting a nsITransferable as an image is:
          // - Use nsITransferable::GetTransferData to fetch the stored data.
          // - QI a nsISupportsInterfacePointer and get the underlying pointer.
          // - QI imgIContainer on the pointer.
          // - Convert the image to the native clipboard format.
          //
          // Below we create a nsITransferable in the above format.
          let imgPtr = new SupportsInterfacePointer();
          imgPtr.data = container;
          let transferable = new Transferable();
          transferable.init(null);
          transferable.addDataFlavor(mimeType);

          // Internal consumers expect the image data to be stored as a
          // nsIInputStream. On Linux and Windows, pasted data is directly
          // retrieved from the system's native clipboard, and made available
          // as a nsIInputStream.
          //
          // On macOS, nsClipboard::GetNativeClipboardData (nsClipboard.mm) uses
          // a cached copy of nsITransferable if available, e.g. when the copy
          // was initiated by the same browser instance. Consequently, the
          // transferable still holds a nsISupportsInterfacePointer pointer
          // instead of a nsIInputStream, and logic that assumes the data to be
          // a nsIInputStream instance fails.
          // For example HTMLEditor::InsertObject (HTMLEditorDataTransfer.cpp)
          // and DataTransferItem::FillInExternalData (DataTransferItem.cpp).
          //
          // As a work-around, we force nsClipboard::GetNativeClipboardData to
          // ignore the cached image data, by passing zero as the length
          // parameter to transferable.setTransferData. When the length is zero,
          // nsITransferable::GetTransferData will return NS_ERROR_FAILURE and
          // conveniently nsClipboard::GetNativeClipboardData will then fall
          // back to retrieving the data directly from the system's clipboard.
          //
          // Note that the length itself is not really used if the data is not
          // a string type, so the actual value does not matter.
          transferable.setTransferData(mimeType, imgPtr, 0);

          Services.clipboard.setData(
            transferable, null, Services.clipboard.kGlobalClipboard);
        },
      },
    };
  }
};
