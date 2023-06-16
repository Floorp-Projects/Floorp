/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { CookieXPCShellUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CookieXPCShellUtils.sys.mjs"
);

CookieXPCShellUtils.init(this);

let gCookieHits = 0;
let gLoadingHits = 0;

add_task(async function () {
  do_get_profile();

  info("Disable predictor and accept all");
  Services.prefs.setBoolPref("network.predictor.enabled", false);
  Services.prefs.setBoolPref("network.predictor.enable-prefetch", false);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  const server = CookieXPCShellUtils.createServer({
    hosts: ["example.org"],
  });
  server.registerPathHandler("/test", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    if (
      request.hasHeader("Cookie") &&
      request.getHeader("Cookie") == "foo=bar"
    ) {
      gCookieHits++;
    } else {
      response.setHeader("Set-Cookie", "foo=bar");
    }

    gLoadingHits++;
    var body = "<html></html>";
    response.bodyOutputStream.write(body, body.length);
  });

  info("Reset the hits count");
  gCookieHits = 0;
  gLoadingHits = 0;

  info("Let's load a page");
  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/test?1"
  );
  await contentPage.close();

  Assert.equal(gCookieHits, 0, "The number of cookie hits match");
  Assert.equal(gLoadingHits, 1, "The number of loading hits match");

  info("Let's load the source of the page again to see if it loads from cache");
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "view-source:http://example.org/test?1"
  );
  await contentPage.close();

  Assert.equal(gCookieHits, 0, "The number of cookie hits match");
  Assert.equal(gLoadingHits, 1, "The number of loading hits match");

  info(
    "Let's load the source of the page without hitting the cache to see if the cookie is sent properly"
  );
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "view-source:http://example.org/test?2"
  );
  await contentPage.close();

  Assert.equal(gCookieHits, 1, "The number of cookie hits match");
  Assert.equal(gLoadingHits, 2, "The number of loading hits match");
});
