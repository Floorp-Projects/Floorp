"use strict";

function setupRedirect(fromUrl, redirectUrl) {
  browser.webRequest.onBeforeRequest.addListener(
    details => {
      console.log(`Extension redirects from ${fromUrl} to ${redirectUrl}`);
      return { redirectUrl };
    },
    { urls: [fromUrl] },
    ["blocking"]
  );
}

// Intercepts all script requests from androidTest/assets/www/trackers.html.
// Scripts are executed in order of appearance in the page and block the
// page's "load" event, so the test runner can just wait for the page to
// have loaded and then check the page content to verify that the requests
// were intercepted as expected.
setupRedirect(
  "http://trackertest.org/tracker.js",
  "data:text/javascript,document.body.textContent='start'"
);
setupRedirect(
  "https://tracking.example.com/tracker.js",
  browser.runtime.getURL("web-accessible-script.js")
);
setupRedirect(
  "https://itisatracker.org/tracker.js",
  `data:text/javascript,document.body.textContent+=',end'`
);

// Work around bug 1300234 to ensure that the webRequest listener has been
// registered before we resume the test. API result doesn't matter, we just
// want to make a roundtrip.
var listenerReady = browser.webRequest.getSecurityInfo("").catch(() => {});

listenerReady.then(() => {
  browser.runtime.sendNativeMessage("browser", "setupReadyStartTest");
});
