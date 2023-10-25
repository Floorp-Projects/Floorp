"use strict";

/* eslint-disable @microsoft/sdl/no-insecure-url */

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
    [false, "Missing expected exception Rej_bad"],
    [true, "Rej_ok"],
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
        await Assert.rejects(Promise.resolve(), /./, "Rej_bad");
        await Assert.rejects(Promise.reject(new Error("k")), /k/, "Rej_ok");
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

        await SpecialPowers.spawn(subFrame, [], async () => {
          Assert.equal(1, 2, "Thing");
          Assert.equal(1, 1, "Hmm");
          Assert.ok(true, "Yay.");
          Assert.ok(false, "Boo!.");
          await Assert.rejects(Promise.resolve(), /./, "Rej_bad");
          await Assert.rejects(Promise.reject(new Error("k")), /k/, "Rej_ok");
        });
      });
    },
    "SpecialPowers.spawnChrome": () => {
      return SpecialPowers.spawnChrome([], async () => {
        Assert.equal(1, 2, "Thing");
        Assert.equal(1, 1, "Hmm");
        Assert.ok(true, "Yay.");
        Assert.ok(false, "Boo!.");
        await Assert.rejects(Promise.resolve(), /./, "Rej_bad");
        await Assert.rejects(Promise.reject(new Error("k")), /k/, "Rej_ok");
      });
    },
    "SpecialPowers.loadChromeScript": async () => {
      let script = SpecialPowers.loadChromeScript(() => {
        /* eslint-env mozilla/chrome-script */
        const resultPromise = (async () => {
          Assert.equal(1, 2, "Thing");
          Assert.equal(1, 1, "Hmm");
          Assert.ok(true, "Yay.");
          Assert.ok(false, "Boo!.");
          await Assert.rejects(Promise.resolve(), /./, "Rej_bad");
          await Assert.rejects(Promise.reject(new Error("k")), /k/, "Rej_ok");
        })();
        this.addMessageListener("ping", () => resultPromise);
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
    for (let { msg, stack } of diags) {
      ok(stack, `Got stack for: ${msg}`);
      // Unlike the html version of this test, this one does not include a "/"
      // in front of the file name, because somehow Android only includes the
      // file name, and not the fuller path.
      let expectedFilenamePart = "test_SpecialPowersSandbox.js:";
      if (name === "SpecialPowers.loadChromeScript") {
        // Unfortunately, the original file name is not included;
        // the function name or a dummy value is used instead.
        expectedFilenamePart = "loadChromeScript anonymous function>:";
      }
      if (!stack.includes(expectedFilenamePart)) {
        ok(false, `Stack does not contain ${expectedFilenamePart}: ${stack}`);
      }
    }
  }
});
