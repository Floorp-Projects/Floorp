/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Common code for PSM scripts in security/manager/tools/.

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

/**
 * Synchronously downloads a file.
 *
 * @param {String} url
 *        The URL to download the file from.
 * @param {Boolean} decodeContentsAsBase64
 *        Whether the downloaded contents should be Base64 decoded before being
 *        returned.
 * @returns {String}
 *          The downloaded contents.
 */
function downloadFile(url, decodeContentsAsBase64) {
  let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", url, false); // doing the request synchronously
  try {
    req.send();
  } catch (e) {
    throw new Error(`ERROR: problem downloading '${url}': ${e}`);
  }

  if (req.status != 200) {
    throw new Error(`ERROR: problem downloading '${url}': status ${req.status}`);
  }

  if (!decodeContentsAsBase64) {
    return req.responseText;
  }

  try {
    return atob(req.responseText);
  } catch (e) {
    throw new Error(`ERROR: could not decode data as base64 from '${url}': ` +
                    e);
  }
}

/**
 * Removes //-style block (but not trailing) comments from a string.
 * @param {String} input
 *        Potentially multi-line input.
 * @returns {String}
 */
function stripComments(input) {
  return input.replace(/^(\s*)?\/\/[^\n]*\n/mg, "");
}

this.PSMToolUtils = { downloadFile, stripComments };
this.EXPORTED_SYMBOLS = ["PSMToolUtils"];
