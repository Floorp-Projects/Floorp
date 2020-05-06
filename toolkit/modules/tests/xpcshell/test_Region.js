"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");

const RESPONSE_DELAY = 500;
const RESPONSE_TIMEOUT = 100;

function useHttpServer() {
  let server = new HttpServer();
  server.start(-1);
  Services.prefs.setCharPref(
    "browser.region.network.url",
    `http://localhost:${server.identity.primaryPort}/geo`
  );
  return server;
}

function send(res, json) {
  res.setStatusLine("1.1", 200, "OK");
  res.setHeader("content-type", "application/json", true);
  res.write(JSON.stringify(json));
}

add_task(async function test_basic() {
  let srv = useHttpServer();

  srv.registerPathHandler("/geo", (req, res) => {
    res.setStatusLine("1.1", 200, "OK");
    send(res, { country_code: "US" });
  });

  let res = await Region.getHomeRegion();
  Assert.ok(true, "Region fetch should succeed");
  Assert.equal(
    res.country_code,
    "US",
    "Region fetch should return correct result"
  );

  await new Promise(r => srv.stop(r));
});

add_task(async function test_timeout() {
  let srv = useHttpServer();

  srv.registerPathHandler("/geo", (req, res) => {
    res.processAsync();
    do_timeout(RESPONSE_DELAY, () => {
      send(res, { country_code: "US" });
      res.finish();
    });
  });

  Services.prefs.setIntPref("browser.region.timeout", RESPONSE_TIMEOUT);

  try {
    await Region.getHomeRegion();
    Assert.ok(false, "Region fetch should have timed out");
  } catch (e) {
    Assert.equal(e.message, "region-fetch-timeout", "Region fetch timed out");
  }

  await new Promise(r => srv.stop(r));
});
