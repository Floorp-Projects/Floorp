/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides access to downloads from previous sessions on platforms that store
 * them in a different location than session downloads.
 *
 * This module works with objects that are compatible with Download, while using
 * the Places interfaces internally. Some of the Places objects may also be
 * exposed to allow the consumers to integrate with history view commands.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadHistory",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

const METADATA_ANNO = "downloads/metaData";

const METADATA_STATE_FINISHED = 1;
const METADATA_STATE_FAILED = 2;
const METADATA_STATE_CANCELED = 3;
const METADATA_STATE_BLOCKED_PARENTAL = 6;
const METADATA_STATE_DIRTY = 8;

/**
 * Provides methods to retrieve downloads from previous sessions and store
 * downloads for future sessions.
 */
this.DownloadHistory = {
  /**
   * Stores new detailed metadata for the given download in history. This is
   * normally called after a download finishes, fails, or is canceled.
   *
   * Failed or canceled downloads with partial data are not stored as paused,
   * because the information from the session download is required for resuming.
   *
   * @param download
   *        Download object whose metadata should be updated. If the object
   *        represents a private download, the call has no effect.
   */
  updateMetaData(download) {
    if (download.source.isPrivate || !download.stopped) {
      return;
    }

    let state = METADATA_STATE_CANCELED;
    if (download.succeeded) {
      state = METADATA_STATE_FINISHED;
    } else if (download.error) {
      if (download.error.becauseBlockedByParentalControls) {
        state = METADATA_STATE_BLOCKED_PARENTAL;
      } else if (download.error.becauseBlockedByReputationCheck) {
        state = METADATA_STATE_DIRTY;
      } else {
        state = METADATA_STATE_FAILED;
      }
    }

    let metaData = { state, endTime: download.endTime };
    if (download.succeeded) {
      metaData.fileSize = download.target.size;
    }

    // The verdict may still be present even if the download succeeded.
    if (download.error && download.error.reputationCheckVerdict) {
      metaData.reputationCheckVerdict =
        download.error.reputationCheckVerdict;
    }

    try {
      PlacesUtils.annotations.setPageAnnotation(
                                 Services.io.newURI(download.source.url),
                                 METADATA_ANNO,
                                 JSON.stringify(metaData), 0,
                                 PlacesUtils.annotations.EXPIRE_WITH_HISTORY);
    } catch (ex) {
      Cu.reportError(ex);
    }
  },
};
