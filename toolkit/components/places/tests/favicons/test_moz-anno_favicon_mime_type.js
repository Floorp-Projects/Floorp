/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test ensures that the mime type is set for moz-anno channels of favicons
 * properly.  Added with work in bug 481227.
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

const testFaviconData = "data:image/png,%89PNG%0D%0A%1A%0A%00%00%00%0DIHDR%00%00%00%10%00%00%00%10%08%06%00%00%00%1F%F3%FFa%00%00%00%04gAMA%00%00%AF%C87%05%8A%E9%00%00%00%19tEXtSoftware%00Adobe%20ImageReadyq%C9e%3C%00%00%01%D6IDATx%DAb%FC%FF%FF%3F%03%25%00%20%80%98%909%EF%DF%BFg%EF%EC%EC%FC%AD%AC%AC%FC%DF%95%91%F1%BF%89%89%C9%7F%20%FF%D7%EA%D5%AB%B7%DF%BBwO%16%9B%01%00%01%C4%00r%01%08%9F9s%C6%CD%D8%D8%F8%BF%0B%03%C3%FF3%40%BC%0A%88%EF%02q%1A%10%BB%40%F1%AAU%ABv%C1%D4%C30%40%00%81%89%993g%3E%06%1A%F6%3F%14%AA%11D%97%03%F1%7Fc%08%0D%E2%2B))%FD%17%04%89%A1%19%00%10%40%0C%D00%F8%0F3%00%C8%F8%BF%1B%E4%0Ac%88a%E5%60%17%19%FF%0F%0D%0D%05%1B%02v%D9%DD%BB%0A0%03%00%02%08%AC%B9%A3%A3%E3%17%03%D4v%90%01%EF%18%106%C3%0Cz%07%C5%BB%A1%DE%82y%07%20%80%A0%A6%08B%FCn%0C1%60%26%D4%20d%C3VA%C3%06%26%BE%0A%EA-%80%00%82%B9%E0%F7L4%0D%EF%90%F8%C6%60%2F%0A%82%BD%01%13%07%0700%D0%01%02%88%11%E4%02P%B41%DC%BB%C7%D0%014%0D%E8l%06W%20%06%BA%88%A1%1C%1AS%15%40%7C%16%CA6.%2Fgx%BFg%0F%83%CB%D9%B3%0C%7B%80%7C%80%00%02%BB%00%E8%9F%ED%20%1B%3A%A0%A6%9F%81%DA%DC%01%C5%B0%80%ED%80%FA%BF%BC%BC%FC%3F%83%12%90%9D%96%F6%1F%20%80%18%DE%BD%7B%C7%0E%8E%05AD%20%FEGr%A6%A0%A0%E0%7F%25P%80%02%9D%0F%D28%13%18%23%C6%C0%B0%02E%3D%C8%F5%00%01%04%8F%05P%A8%BA%40my%87%E4%12c%A8%8D%20%8B%D0%D3%00%08%03%04%10%9C%01R%E4%82d%3B%C8%A0%99%C6%90%90%C6%A5%19%84%01%02%08%9E%17%80%C9x%F7%7B%A0%DBVC%F9%A0%C0%5C%7D%16%2C%CE%00%F4%C6O%5C%99%09%20%800L%04y%A5%03%1A%95%A0%80%05%05%14.%DBA%18%20%80%18)%CD%CE%00%01%06%00%0C'%94%C7%C0k%C9%2C%00%00%00%00IEND%AEB%60%82";
const moz_anno_favicon_prefix = "moz-anno:favicon:";

////////////////////////////////////////////////////////////////////////////////
//// streamListener

function streamListener(aExpectedContentType)
{
  this._expectedContentType = aExpectedContentType;
}
streamListener.prototype =
{
  onStartRequest: function(aRequest, aContext)
  {
    // We have other tests that make sure the data is what we expect.  We just
    // need to check the content type here.
    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    dump("*** Checking " + channel.URI.spec + "\n");
    do_check_eq(channel.contentType, this._expectedContentType);

    // If we somehow throw before doing the above check, the test will pass, so
    // we do this for extra sanity.
    this._checked = true;
  },
  onStopRequest: function()
  {
    do_check_true(this._checked);
    do_test_finished();
  },
  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount)
  {
    aRequest.cancel(Cr.NS_ERROR_ABORT);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

function run_test()
{
  let fs = Cc["@mozilla.org/browser/favicon-service;1"].
           getService(Ci.nsIFaviconService);
  let ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);

  // Test that the default icon has the content type of image/png.
  let channel = ios.newChannelFromURI2(fs.defaultFavicon,
                                        null,      // aLoadingNode
                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                        null,      // aTriggeringPrincipal
                                        Ci.nsILoadInfo.SEC_NORMAL,
                                        Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE);
  channel.asyncOpen(new streamListener("image/png"), null);
  do_test_pending();

  // Test URI that we don't know anything about.  Will end up being the default
  // icon, so expect image/png.
  channel = ios.newChannel2(moz_anno_favicon_prefix + "http://mozilla.org",
                            null,
                            null,
                            null,      // aLoadingNode
                            Services.scriptSecurityManager.getSystemPrincipal(),
                            null,      // aTriggeringPrincipal
                            Ci.nsILoadInfo.SEC_NORMAL,
                            Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE);
  channel.asyncOpen(new streamListener("image/png"), null);
  do_test_pending();

  // Test that the content type of a favicon we add ends up being image/png.
  let testURI = uri("http://mozilla.org/");
  // Add the data before opening
  fs.replaceFaviconDataFromDataURL(testURI, testFaviconData,
                                   (Date.now() + 60 * 60 * 24 * 1000) * 1000,
                                   Services.scriptSecurityManager.getSystemPrincipal());

  // Open the channel
  channel = ios.newChannel2(moz_anno_favicon_prefix + testURI.spec,
                            null,
                            null,
                            null,      // aLoadingNode
                            Services.scriptSecurityManager.getSystemPrincipal(),
                            null,      // aTriggeringPrincipal
                            Ci.nsILoadInfo.SEC_NORMAL,
                            Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE);
  channel.asyncOpen(new streamListener("image/png"), null);
  do_test_pending();
}
