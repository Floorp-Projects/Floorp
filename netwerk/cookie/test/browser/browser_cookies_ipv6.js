"use strict";

let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let gHttpServer = null;
let ip = "[::1]";

function contentHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);
  let body = `
    <!DOCTYPE HTML>
      <html>
        <head>
          <meta charset='utf-8'>
          <title>Cookie ipv6 Test</title>
        </head>
        <body>
        </body>
    </html>`;
  response.bodyOutputStream.write(body, body.length);
}

add_task(async _ => {
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    gHttpServer.registerPathHandler("/content", contentHandler);
    gHttpServer._start(-1, ip);
  }

  registerCleanupFunction(() => {
    gHttpServer.stop(() => {
      gHttpServer = null;
    });
  });

  let serverPort = gHttpServer.identity.primaryPort;
  let testURL = `http://${ip}:${serverPort}/content`;

  // Let's open our tab.
  const tab = BrowserTestUtils.addTab(gBrowser, testURL);
  gBrowser.selectedTab = tab;

  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Test if we can set and get document.cookie successfully.
  await SpecialPowers.spawn(browser, [], () => {
    content.document.cookie = "foo=bar";
    is(content.document.cookie, "foo=bar");
  });

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});
