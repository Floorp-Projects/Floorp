"use strict";

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

XPCShellContentUtils.init(this);

const HTML = String.raw`<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title></title>
</head>
<body>
  <span id="span">Hello there.</span>
</body>
</html>`;

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com"],
});

server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(HTML);
});

add_task(async function () {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let page = await XPCShellContentUtils.loadContentPage("http://example.com/", {
    remote: true,
    remoteSubframes: true,
  });

  let { SpecialPowers, browsingContext } = page;

  let result = await SpecialPowers.spawn(
    browsingContext,
    ["#span"],
    selector => {
      let elem = content.document.querySelector(selector);
      return elem.textContent;
    }
  );

  equal(result, "Hello there.", "Got correct element text from frame");

  let line = Components.stack.lineNumber + 1;
  let callback = () => {
    let e = new Error("Hello.");
    return { filename: e.fileName, lineNumber: e.lineNumber };
  };

  let loc = await SpecialPowers.spawn(browsingContext, [], callback);
  equal(
    loc.filename,
    Components.stack.filename,
    "Error should have correct script filename"
  );
  equal(
    loc.lineNumber,
    line + 2,
    "Error should have correct script line number"
  );

  await page.close();
});
