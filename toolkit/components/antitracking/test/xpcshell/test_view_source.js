/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

CookieXPCShellUtils.init(this);

let gHits = 0;

add_task(async function() {
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
      gHits++;
    } else {
      response.setHeader("Set-Cookie", "foo=bar");
    }
    var body = "<html></html>";
    response.bodyOutputStream.write(body, body.length);
  });

  info("Reset the hits count");
  gHits = 0;

  info("Let's load a page");
  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/test?1"
  );
  await contentPage.close();

  Assert.equal(gHits, 0, "The number of hits match");

  info("Let's load the source of the page");
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "view-source:http://example.org/test?2"
  );
  await contentPage.close();

  Assert.equal(gHits, 1, "The number of hits match");
});
