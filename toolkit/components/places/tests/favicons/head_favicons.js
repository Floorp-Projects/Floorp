/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

// This error icon must stay in sync with FAVICON_ERRORPAGE_URL in
// nsIFaviconService.idl, aboutCertError.xhtml and netError.xhtml.
const FAVICON_ERRORPAGE_URI =
  Services.io.newURI("chrome://global/skin/icons/warning-16.png");

/**
 * Waits for the first OnPageChanged notification for ATTRIBUTE_FAVICON, and
 * verifies that it matches the expected page URI and associated favicon URI.
 *
 * This function also double-checks the GUID parameter of the notification.
 *
 * @param aExpectedPageURI
 *        nsIURI object of the page whose favicon should change.
 * @param aExpectedFaviconURI
 *        nsIURI object of the newly associated favicon.
 * @param aCallback
 *        This function is called after the check finished.
 */
function waitForFaviconChanged(aExpectedPageURI, aExpectedFaviconURI,
                               aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onPageChanged: function WFFC_onPageChanged(aURI, aWhat, aValue, aGUID) {
      if (aWhat != Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
        return;
      }
      PlacesUtils.history.removeObserver(this);

      do_check_true(aURI.equals(aExpectedPageURI));
      do_check_eq(aValue, aExpectedFaviconURI.spec);
      do_check_guid_for_uri(aURI, aGUID);
      aCallback();
    }
  };
  PlacesUtils.history.addObserver(historyObserver);
}

/**
 * Checks that the favicon for the given page matches the provided data.
 *
 * @param aPageURI
 *        nsIURI object for the page to check.
 * @param aExpectedMimeType
 *        Expected MIME type of the icon, for example "image/png".
 * @param aExpectedData
 *        Expected icon data, expressed as an array of byte values.
 * @param aCallback
 *        This function is called after the check finished.
 */
function checkFaviconDataForPage(aPageURI, aExpectedMimeType, aExpectedData,
                                 aCallback) {
  PlacesUtils.favicons.getFaviconDataForPage(aPageURI,
    function(aURI, aDataLen, aData, aMimeType) {
      do_check_eq(aExpectedMimeType, aMimeType);
      do_check_true(compareArrays(aExpectedData, aData));
      do_check_guid_for_uri(aPageURI);
      aCallback();
    });
}

/**
 * Checks that the given page has no associated favicon.
 *
 * @param aPageURI
 *        nsIURI object for the page to check.
 * @param aCallback
 *        This function is called after the check finished.
 */
function checkFaviconMissingForPage(aPageURI, aCallback) {
  PlacesUtils.favicons.getFaviconURLForPage(aPageURI,
    function(aURI, aDataLen, aData, aMimeType) {
      do_check_true(aURI === null);
      aCallback();
    });
}

function promiseFaviconMissingForPage(aPageURI) {
  return new Promise(resolve => checkFaviconMissingForPage(aPageURI, resolve));
}

function promiseFaviconChanged(aExpectedPageURI, aExpectedFaviconURI) {
  return new Promise(resolve => waitForFaviconChanged(aExpectedPageURI, aExpectedFaviconURI, resolve));
}
