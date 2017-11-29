/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LightweightThemeImageOptimizer"];

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

const ORIGIN_TOP_RIGHT = 1;
const ORIGIN_BOTTOM_LEFT = 2;

this.LightweightThemeImageOptimizer = {
  optimize(aThemeData, aScreen) {
    let data = Object.assign({}, aThemeData);
    if (!data.headerURL) {
      return data;
    }

    data.headerURL = ImageCropper.getCroppedImageURL(
      data.headerURL, aScreen, ORIGIN_TOP_RIGHT);

    if (data.footerURL) {
      data.footerURL = ImageCropper.getCroppedImageURL(
        data.footerURL, aScreen, ORIGIN_BOTTOM_LEFT);
    }

    return data;
  },

  purge() {
    let dir = FileUtils.getDir("ProfD", ["lwtheme"]);
    dir.followLinks = false;
    try {
      dir.remove(true);
    } catch (e) {}
  }
};

Object.freeze(LightweightThemeImageOptimizer);

var ImageCropper = {
  _inProgress: {},

  getCroppedImageURL(aImageURL, aScreen, aOrigin) {
    // We can crop local files, only.
    if (!aImageURL.startsWith("file://")) {
      return aImageURL;
    }

    // Generate the cropped image's file name using its
    // base name and the current screen size.
    let uri = Services.io.newURI(aImageURL);
    let file = uri.QueryInterface(Ci.nsIFileURL).file;

    // Make sure the source file exists.
    if (!file.exists()) {
      return aImageURL;
    }

    let fileName = file.leafName + "-" + aScreen.width + "x" + aScreen.height;
    let croppedFile = FileUtils.getFile("ProfD", ["lwtheme", fileName]);

    // If we have a local file that is not in progress, return it.
    if (croppedFile.exists() && !(croppedFile.path in this._inProgress)) {
      let fileURI = Services.io.newFileURI(croppedFile);

      // Copy the query part to avoid wrong caching.
      fileURI.QueryInterface(Ci.nsIURL).query = uri.query;
      return fileURI.spec;
    }

    // Crop the given image in the background.
    this._crop(uri, croppedFile, aScreen, aOrigin);

    // Return the original image while we're waiting for the cropped version
    // to be written to disk.
    return aImageURL;
  },

  _crop(aURI, aTargetFile, aScreen, aOrigin) {
    let inProgress = this._inProgress;
    inProgress[aTargetFile.path] = true;

    function resetInProgress() {
      delete inProgress[aTargetFile.path];
    }

    ImageFile.read(aURI, function(aInputStream, aContentType) {
      if (aInputStream && aContentType) {
        ImageTools.decode(aInputStream, aContentType, function(aImage) {
          if (aImage && aImage.width && aImage.height) {
            let stream = ImageTools.encode(aImage, aScreen, aOrigin, aContentType);
            if (stream) {
              ImageFile.write(aTargetFile, stream, resetInProgress);
            }
          }
        });
      }

      resetInProgress();
    });
  }
};

var ImageFile = {
  read(aURI, aCallback) {
    this._netUtil.asyncFetch({
      uri: aURI,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
    }, function(aInputStream, aStatus, aRequest) {
      if (Components.isSuccessCode(aStatus) && aRequest instanceof Ci.nsIChannel) {
        let channel = aRequest.QueryInterface(Ci.nsIChannel);
        aCallback(aInputStream, channel.contentType);
      } else {
        aCallback();
      }
    });
  },

  write(aFile, aInputStream, aCallback) {
    let fos = FileUtils.openSafeFileOutputStream(aFile);
    this._netUtil.asyncCopy(aInputStream, fos, function(aResult) {
      FileUtils.closeSafeFileOutputStream(fos);

      // Remove the file if writing was not successful.
      if (!Components.isSuccessCode(aResult)) {
        try {
          aFile.remove(false);
        } catch (e) {}
      }

      aCallback();
    });
  }
};

XPCOMUtils.defineLazyModuleGetter(ImageFile, "_netUtil",
                                  "resource://gre/modules/NetUtil.jsm", "NetUtil");

var ImageTools = {
  decode(aInputStream, aContentType, aCallback) {
    let callback = {
      onImageReady(aImage, aStatus) {
        aCallback(aImage);
      }
    };

    try {
      let threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
      this._imgTools.decodeImageAsync(aInputStream, aContentType, callback,
                                      threadManager.currentThread);
    } catch (e) {
      aCallback(null);
    }
  },

  encode(aImage, aScreen, aOrigin, aContentType) {
    let stream;
    let width = Math.min(aImage.width, aScreen.width);
    let height = Math.min(aImage.height, aScreen.height);
    let x = aOrigin == ORIGIN_TOP_RIGHT ? aImage.width - width : 0;

    try {
      stream = this._imgTools.encodeCroppedImage(aImage, aContentType, x, 0,
                                                 width, height);
    } catch (e) {}

    return stream;
  }
};

XPCOMUtils.defineLazyServiceGetter(ImageTools, "_imgTools",
                                   "@mozilla.org/image/tools;1", "imgITools");

