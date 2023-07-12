/*
Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

function test() {
  waitForExplicitFinish();

  // We overload this test to include verifying that httpd.js is
  // importable as a testing-only JS module.
  ChromeUtils.importESModule("resource://testing-common/httpd.sys.mjs");

  nextTest();
}

function nextTest() {
  if (tests.length) {
    executeSoon(tests.shift());
  } else {
    executeSoon(finish);
  }
}

var tests = [test_asyncFetchBadCert];

function test_asyncFetchBadCert() {
  // Try a load from an untrusted cert, with errors supressed
  NetUtil.asyncFetch(
    {
      uri: "https://untrusted.example.com",
      loadUsingSystemPrincipal: true,
    },
    function (aInputStream, aStatusCode, aRequest) {
      ok(!Components.isSuccessCode(aStatusCode), "request failed");
      ok(aRequest instanceof Ci.nsIHttpChannel, "request is an nsIHttpChannel");

      // Now try again with a channel whose notificationCallbacks doesn't suprress errors
      let channel = NetUtil.newChannel({
        uri: "https://untrusted.example.com",
        loadUsingSystemPrincipal: true,
      });
      channel.notificationCallbacks = {
        QueryInterface: ChromeUtils.generateQI([
          "nsIProgressEventSink",
          "nsIInterfaceRequestor",
        ]),
        getInterface(aIID) {
          return this.QueryInterface(aIID);
        },
        onProgress() {},
        onStatus() {},
      };
      NetUtil.asyncFetch(
        channel,
        function (aInputStream, aStatusCode, aRequest) {
          ok(!Components.isSuccessCode(aStatusCode), "request failed");
          ok(
            aRequest instanceof Ci.nsIHttpChannel,
            "request is an nsIHttpChannel"
          );

          // Now try a valid request
          NetUtil.asyncFetch(
            {
              uri: "https://example.com",
              loadUsingSystemPrincipal: true,
            },
            function (aInputStream, aStatusCode, aRequest) {
              info("aStatusCode for valid request: " + aStatusCode);
              ok(Components.isSuccessCode(aStatusCode), "request succeeded");
              ok(
                aRequest instanceof Ci.nsIHttpChannel,
                "request is an nsIHttpChannel"
              );
              ok(aRequest.requestSucceeded, "HTTP request succeeded");

              nextTest();
            }
          );
        }
      );
    }
  );
}

function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow(aXULWindow) {
    var domwindow = aXULWindow.docShell.domWindow;
    var self = this;
    domwindow.addEventListener(
      "load",
      function () {
        if (domwindow.document.location.href != self.url) {
          return;
        }

        // Allow other window load listeners to execute before passing to callback
        executeSoon(function () {
          self.callback(domwindow);
        });
      },
      { once: true }
    );
  },
  onCloseWindow(aXULWindow) {},
};
