/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { HPKEConfigManager } = ChromeUtils.importESModule(
  "resource://gre/modules/HPKEConfigManager.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let gHttpServer;
let gValidRequestCount = 0;
let gFickleIsWorking = true;

add_setup(async function () {
  gHttpServer = new HttpServer();
  let invalidHandler = (req, res) => {
    res.setStatusLine(req.httpVersion, 500, "Oh no, it broke");
    res.write("Uh oh, it broke.");
  };
  let validHandler = (req, res) => {
    res.setHeader("Content-Type", "application/ohttp-keys");
    res.write("1234");
    gValidRequestCount++;
  };

  gHttpServer.registerPathHandler("/.wellknown/invalid", invalidHandler);
  gHttpServer.registerPathHandler("/.wellknown/valid", validHandler);

  gHttpServer.registerPathHandler("/.wellknown/fickle", (req, res) => {
    if (gFickleIsWorking) {
      return validHandler(req, res);
    }
    return invalidHandler(req, res);
  });

  gHttpServer.start(-1);
});

function getLocalURL(path) {
  return `http://localhost:${gHttpServer.identity.primaryPort}/.wellknown/${path}`;
}

add_task(async function test_broken_url_returns_null() {
  Assert.equal(await HPKEConfigManager.get(getLocalURL("invalid")), null);
});

add_task(async function test_working_url_returns_data() {
  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("valid")),
    new TextEncoder().encode("1234")
  );
});

add_task(async function test_we_only_request_once() {
  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("valid")),
    new TextEncoder().encode("1234")
  );
  let oldRequestCount = gValidRequestCount;

  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("valid")),
    new TextEncoder().encode("1234")
  );
  Assert.equal(
    oldRequestCount,
    gValidRequestCount,
    "Shouldn't have made another request."
  );
});

add_task(async function test_maxAge_forces_refresh() {
  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("valid")),
    new TextEncoder().encode("1234")
  );
  let oldRequestCount = gValidRequestCount;

  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("valid"), { maxAge: 0 }),
    new TextEncoder().encode("1234")
  );
  Assert.equal(
    oldRequestCount + 1,
    gValidRequestCount,
    "Should have made another request due to maxAge."
  );
});

add_task(async function test_maxAge_handling_of_invalid_requests() {
  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("fickle")),
    new TextEncoder().encode("1234")
  );

  gFickleIsWorking = false;

  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("fickle"), { maxAge: 0 }),
    null
  );

  Assert.deepEqual(
    await HPKEConfigManager.get(getLocalURL("fickle")),
    new TextEncoder().encode("1234"),
    "Should still have the cached config if no max age is passed."
  );
});
