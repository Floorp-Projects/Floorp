/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

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
function checkFaviconDataForPage(
  aPageURI,
  aExpectedMimeType,
  aExpectedData,
  aCallback
) {
  PlacesUtils.favicons.getFaviconDataForPage(
    aPageURI,
    async function (aURI, aDataLen, aData, aMimeType) {
      Assert.equal(aExpectedMimeType, aMimeType);
      Assert.ok(compareArrays(aExpectedData, aData));
      await check_guid_for_uri(aPageURI);
      aCallback();
    }
  );
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
  PlacesUtils.favicons.getFaviconURLForPage(
    aPageURI,
    function (aURI, aDataLen, aData, aMimeType) {
      Assert.ok(aURI === null);
      aCallback();
    }
  );
}

function promiseFaviconMissingForPage(aPageURI) {
  return new Promise(resolve => checkFaviconMissingForPage(aPageURI, resolve));
}

function promiseFaviconChanged(aExpectedPageURI, aExpectedFaviconURI) {
  return new Promise(resolve => {
    PlacesTestUtils.waitForNotification("favicon-changed", async events => {
      for (let e of events) {
        if (e.url == aExpectedPageURI.spec) {
          Assert.equal(e.faviconUrl, aExpectedFaviconURI.spec);
          await check_guid_for_uri(aExpectedPageURI, e.pageGuid);
          resolve();
        }
      }
    });
  });
}
