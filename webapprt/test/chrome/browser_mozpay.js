Cu.import("resource://gre/modules/Services.jsm");
let { PaymentManager } = Cu.import("resource://gre/modules/Payment.jsm", {});

function test() {
  waitForExplicitFinish();

  let providerWindow = null;
  let providerUri = "https://example.com:443/webapprtChrome/webapprt/test/chrome/mozpay-success.html?req=";
  let jwt = "eyJhbGciOiAiSFMyNTYiLCAidHlwIjogIkpXVCJ9.eyJhdWQiOiAibW9j" +
            "a3BheXByb3ZpZGVyLnBocGZvZ2FwcC5jb20iLCAiaXNzIjogIkVudGVyI" +
            "HlvdSBhcHAga2V5IGhlcmUhIiwgInJlcXVlc3QiOiB7Im5hbWUiOiAiUG" +
            "llY2Ugb2YgQ2FrZSIsICJwcmljZSI6ICIxMC41MCIsICJwcmljZVRpZXI" +
            "iOiAxLCAicHJvZHVjdGRhdGEiOiAidHJhbnNhY3Rpb25faWQ9ODYiLCAi" +
            "Y3VycmVuY3lDb2RlIjogIlVTRCIsICJkZXNjcmlwdGlvbiI6ICJWaXJ0d" +
            "WFsIGNob2NvbGF0ZSBjYWtlIHRvIGZpbGwgeW91ciB2aXJ0dWFsIHR1bW" +
            "15In0sICJleHAiOiAxMzUyMjMyNzkyLCAiaWF0IjogMTM1MjIyOTE5Miw" +
            "gInR5cCI6ICJtb2NrL3BheW1lbnRzL2luYXBwL3YxIn0.QZxc62USCy4U" +
            "IyKIC1TKelVhNklvk-Ou1l_daKntaFI";

  PaymentManager.registeredProviders = {};
  PaymentManager.registeredProviders["mock/payments/inapp/v1"] = {
    name: "mockprovider",
    description: "Mock Payment Provider",
    uri: providerUri,
    requestMethod: "GET"
  };

  let winObserver = function(win, topic) {
    if (topic == "domwindowopened") {
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);

        if (win.document.getElementById("content").getAttribute("src") ==
            (providerUri + jwt)) {
          ok(true, "Payment provider window shown.");
          providerWindow = win;
        }
      }, false);
    }
  }

  Services.ww.registerNotification(winObserver);

  let mutObserver = null;

  loadWebapp("mozpay.webapp", undefined, function onLoad() {
    let msg = gAppBrowser.contentDocument.getElementById("msg");
    mutObserver = new MutationObserver(function(mutations) {
      if (msg.textContent == "Success.") {
        ok(true, "Payment success.");
      } else {
        ok(false, "Payment success.");
      }

      if (providerWindow == null) {
        ok(false, "Payment provider window shown.");
      } else {
        providerWindow.close();
      }

      finish();
    });
    mutObserver.observe(msg, { childList: true });
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    mutObserver.disconnect();
  });
}
