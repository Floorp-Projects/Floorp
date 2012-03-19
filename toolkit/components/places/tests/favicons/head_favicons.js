/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
let (commonFile = do_get_file("../head_common.js", false)) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.


// This error icon must stay in sync with FAVICON_ERRORPAGE_URL in
// nsIFaviconService.idl, aboutCertError.xhtml and netError.xhtml.
const FAVICON_ERRORPAGE_URI =
  NetUtil.newURI("chrome://global/skin/icons/warning-16.png");

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
  PlacesUtils.history.addObserver(historyObserver, false);
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
    function CFDFP_onFaviconDataAvailable(aURI, aDataLen, aData, aMimeType) {
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
  // Ask for the favicon associated with the page, expecting not to be notified.
  let notificationReceived = false;
  PlacesUtils.favicons.getFaviconURLForPage(aPageURI,
    function CFMFP_onFaviconDataAvailable() {
      notificationReceived = true;
    });

  // We must wait for the asynchronous database thread to finish the operation,
  // and then for the main thread to process any pending notifications that came
  // from the asynchronous thread, before we can be sure that
  // onFaviconDataAvailable was not invoked in the meantime.
  waitForAsyncUpdates(function CFMFP_asyncUpdates() {
    do_execute_soon(function CFMFP_soon() {
      do_check_false(notificationReceived);
      aCallback();
    });
  });
}
