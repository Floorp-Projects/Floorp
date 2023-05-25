"use strict";

/* eslint-disable @microsoft/sdl/no-insecure-url */

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
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
  hosts: ["example.com", "example.org"],
});

server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(HTML);
});
/**
 * Tests that the shared sandbox functionality for cross-process script
 * execution works as expected. In particular, ensures that Assert methods
 * report the correct diagnostics in the caller scope.
 */

let scope = this;

async function interceptDiagnostics(func) {
  let originalRecord = scope.do_report_result;
  try {
    let diags = [];

    scope.do_report_result = (passed, msg, stack) => {
      diags.push({ passed, msg, stack });
    };

    await func();

    return diags;
  } finally {
    scope.do_report_result = originalRecord;
  }
}

add_task(async function () {
  const frameSrc = "http://example.com/";
  const subframeSrc = "http://example.org/";

  let page = await XPCShellContentUtils.loadContentPage(frameSrc, {
    remote: true,
    remoteSubframes: true,
  });

  let { SpecialPowers, browsingContext } = page;

  let expected = [
    [false, "Thing - 1 == 2"],
    [true, "Hmm - 1 == 1"],
    [true, "Yay. - true == true"],
    [false, "Boo!. - false == true"],
  ];

  // Test that a representative variety of assertions work as expected, and
  // trigger the expected calls to the harness's reporting function.
  //
  // Note: Assert.sys.mjs has its own tests, and defers all of its reporting to a
  // single reporting function, so we don't need to test it comprehensively. We
  // just need to make sure that the general functionality works as expected.
  let tests = {
    "SpecialPowers.spawn": () => {
      return SpecialPowers.spawn(browsingContext, [], async () => {
        Assert.equal(1, 2, "Thing");
        Assert.equal(1, 1, "Hmm");
        Assert.ok(true, "Yay.");
        Assert.ok(false, "Boo!.");
      });
    },
    "SpecialPowers.spawn-subframe": () => {
      return SpecialPowers.spawn(browsingContext, [subframeSrc], async src => {
        let subFrame = this.content.document.createElement("iframe");
        subFrame.src = src;
        this.content.document.body.appendChild(subFrame);

        await new Promise(resolve => {
          subFrame.addEventListener("load", resolve, { once: true });
        });

        await SpecialPowers.spawn(subFrame, [], () => {
          Assert.equal(1, 2, "Thing");
          Assert.equal(1, 1, "Hmm");
          Assert.ok(true, "Yay.");
          Assert.ok(false, "Boo!.");
        });
      });
    },
    "SpecialPowers.spawnChrome": () => {
      return SpecialPowers.spawnChrome([], async () => {
        Assert.equal(1, 2, "Thing");
        Assert.equal(1, 1, "Hmm");
        Assert.ok(true, "Yay.");
        Assert.ok(false, "Boo!.");
      });
    },
    "SpecialPowers.loadChromeScript": async () => {
      let script = SpecialPowers.loadChromeScript(() => {
        /* eslint-env mozilla/chrome-script */
        this.addMessageListener("ping", () => "pong");

        Assert.equal(1, 2, "Thing");
        Assert.equal(1, 1, "Hmm");
        Assert.ok(true, "Yay.");
        Assert.ok(false, "Boo!.");
      });

      await script.sendQuery("ping");
      script.destroy();
    },
  };

  for (let [name, func] of Object.entries(tests)) {
    info(`Starting task: ${name}`);

    let diags = await interceptDiagnostics(func);

    let results = diags.map(diag => [diag.passed, diag.msg]);

    deepEqual(results, expected, "Got expected assertions");
  }
});
