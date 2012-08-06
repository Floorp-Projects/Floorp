/*
Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/
*/

Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();

  // We overload this test to include verifying that httpd.js is
  // importable as a testing-only JS module.
  Components.utils.import("resource://testing-common/httpd.js", {});

  nextTest();
}

function nextTest() {
  if (tests.length)
    executeSoon(tests.shift());
  else
    executeSoon(finish);
}

var tests = [
  test_asyncFetchBadCert,
];

var gCertErrorDialogShown = 0;

// We used to show a dialog box by default when we encountered an SSL
// certificate error. Now we treat these errors just like other
// networking errors; the dialog is no longer shown.
function test_asyncFetchBadCert() {
  let listener = new WindowListener("chrome://pippki/content/certerror.xul", function (domwindow) {
    gCertErrorDialogShown++;

    // Close the dialog
    domwindow.document.documentElement.cancelDialog();
  });

  Services.wm.addListener(listener);

  // Try a load from an untrusted cert, with errors supressed
  NetUtil.asyncFetch("https://untrusted.example.com", function (aInputStream, aStatusCode, aRequest) {
    ok(!Components.isSuccessCode(aStatusCode), "request failed");
    ok(aRequest instanceof Ci.nsIHttpChannel, "request is an nsIHttpChannel");

    is(gCertErrorDialogShown, 0, "cert error dialog was not shown");

    // Now try again with a channel whose notificationCallbacks doesn't suprress errors
    let channel = NetUtil.newChannel("https://untrusted.example.com");
    channel.notificationCallbacks = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIProgressEventSink,
                                             Ci.nsIInterfaceRequestor]),
      getInterface: function (aIID) this.QueryInterface(aIID),
      onProgress: function () {},
      onStatus: function () {}
    };
    NetUtil.asyncFetch(channel, function (aInputStream, aStatusCode, aRequest) {
      ok(!Components.isSuccessCode(aStatusCode), "request failed");
      ok(aRequest instanceof Ci.nsIHttpChannel, "request is an nsIHttpChannel");

      is(gCertErrorDialogShown, 0, "cert error dialog was not shown");

      // Now try a valid request
      NetUtil.asyncFetch("https://example.com", function (aInputStream, aStatusCode, aRequest) {
        info("aStatusCode for valid request: " + aStatusCode);
        ok(Components.isSuccessCode(aStatusCode), "request succeeded");
        ok(aRequest instanceof Ci.nsIHttpChannel, "request is an nsIHttpChannel");
        ok(aRequest.requestSucceeded, "HTTP request succeeded");
  
        is(gCertErrorDialogShown, 0, "cert error dialog was not shown");

        Services.wm.removeListener(listener);
        nextTest();
      });
    });

  });
}

function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow: function(aXULWindow) {
    var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    var self = this;
    domwindow.addEventListener("load", function() {
      domwindow.removeEventListener("load", arguments.callee, false);

      if (domwindow.document.location.href != self.url)
        return;

      // Allow other window load listeners to execute before passing to callback
      executeSoon(function() {
        self.callback(domwindow);
      });
    }, false);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {}
}
