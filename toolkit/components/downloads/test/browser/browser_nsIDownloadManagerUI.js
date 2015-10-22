/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var Cr = Components.results;

function test_visibility_open()
{
  var dmui = Cc["@mozilla.org/download-manager-ui;1"].
             getService(Ci.nsIDownloadManagerUI);
  is(dmui.visible, true,
     "nsIDownloadManagerUI indicates that the UI is visible");
}

function test_getAttention_opened()
{
  var dmui = Cc["@mozilla.org/download-manager-ui;1"].
             getService(Ci.nsIDownloadManagerUI);

  // switch focus to this window
  window.focus();

  dmui.getAttention();
  is(dmui.visible, true,
     "nsIDownloadManagerUI indicates that the UI is visible");
}

function test_visibility_closed(aWin)
{
  var dmui = Cc["@mozilla.org/download-manager-ui;1"].
             getService(Ci.nsIDownloadManagerUI);
  aWin.close();
  is(dmui.visible, false,
     "nsIDownloadManagerUI indicates that the UI is not visible");
}

function test_getAttention_closed()
{
  var dmui = Cc["@mozilla.org/download-manager-ui;1"].
             getService(Ci.nsIDownloadManagerUI);

  var exceptionCaught = false;
  try {
    dmui.getAttention();
  } catch (e) {
    is(e.result, Cr.NS_ERROR_UNEXPECTED,
       "Proper exception was caught");
    exceptionCaught = true;
  } finally {
    is(exceptionCaught, true,
       "Exception was caught, as expected");
  }
}

var testFuncs = [
    test_visibility_open
  , test_getAttention_opened
  , test_visibility_closed /* all tests after this *must* expect there to be
                              no open window, otherwise they will fail! */
  , test_getAttention_closed
];

function test()
{
  try {
    if (Services.prefs.getBoolPref("browser.download.useJSTransfer")) {
      return;
    }
  } catch (ex) { }

  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var db = dm.DBConnection;

  // First, we populate the database with some fake data
  db.executeSimpleSQL("DELETE FROM moz_downloads");

  // See if the DM is already open, and if it is, close it!
  var win = Services.wm.getMostRecentWindow("Download:Manager");
  if (win)
    win.close();

  // Ensure that the download manager callbacks and nsIDownloadManagerUI always
  // use the window UI instead of the panel in the browser's window.
  Services.prefs.setBoolPref("browser.download.useToolkitUI", true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.download.useToolkitUI");
  });

  // OK, now that all the data is in, let's pull up the UI
  Cc["@mozilla.org/download-manager-ui;1"].
  getService(Ci.nsIDownloadManagerUI).show();

  // The window doesn't open once we call show, so we need to wait a little bit
  function finishUp() {
    var win = Services.wm.getMostRecentWindow("Download:Manager");

    // Now we can run our tests
    for (var t of testFuncs)
      t(win);

    finish();
  }
  
  waitForExplicitFinish();
  window.setTimeout(finishUp, 1000);
}
