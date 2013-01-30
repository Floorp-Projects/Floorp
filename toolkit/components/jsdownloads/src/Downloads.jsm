/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Main entry point to get references to all the back-end objects.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Downloads",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DownloadCore.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadIntegration",
                                  "resource://gre/modules/DownloadIntegration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadList",
                                  "resource://gre/modules/DownloadList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadStore",
                                  "resource://gre/modules/DownloadStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUIHelper",
                                  "resource://gre/modules/DownloadUIHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Downloads

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides the only entry point to get references to back-end objects.
 */
this.Downloads = {
  /**
   * Creates a new Download object.
   *
   * @param aProperties
   *        Provides the initial properties for the newly created download.
   *        {
   *          source: {
   *            uri: The nsIURI for the download source.
   *          },
   *          target: {
   *            file: The nsIFile for the download target.
   *          },
   *          saver: {
   *            type: String representing the class of download operation
   *                  handled by this saver object, for example "copy".
   *          },
   *        }
   *
   * @return {Promise}
   * @resolves The newly created Download object.
   * @rejects JavaScript exception.
   */
  createDownload: function D_createDownload(aProperties)
  {
    return Task.spawn(function task_D_createDownload() {
      let download = new Download();

      download.source = new DownloadSource();
      download.source.uri = aProperties.source.uri;
      download.target = new DownloadTarget();
      download.target.file = aProperties.target.file;

      // Support for different aProperties.saver values isn't implemented yet.
      download.saver = new DownloadCopySaver();
      download.saver.download = download;

      // This explicitly makes this function a generator for Task.jsm, so that
      // exceptions in the above calls can be reported asynchronously.
      yield;
      throw new Task.Result(download);
    });
  },

  /**
   * Downloads data from a remote network location to a local file.
   *
   * This download method does not provide user interface or the ability to
   * cancel the download programmatically.  For that, you should obtain a
   * reference to a Download object using the createDownload function.
   *
   * @param aSource
   *        The nsIURI for the download source, or alternative DownloadSource.
   * @param aTarget
   *        The nsIFile for the download target, or alternative DownloadTarget.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  simpleDownload: function D_simpleDownload(aSource, aTarget) {
    // Wrap the arguments into simple objects resembling DownloadSource and
    // DownloadTarget, if they are not objects of that type already.
    if (aSource instanceof Ci.nsIURI) {
      aSource = { uri: aSource };
    }
    if (aTarget instanceof Ci.nsIFile) {
      aTarget = { file: aTarget };
    }

    // Create and start the actual download.
    return this.createDownload({
      source: aSource,
      target: aTarget,
      saver: { type: "copy" },
    }).then(function D_SD_onSuccess(aDownload) {
      return aDownload.start();
    });
  },
};
