const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
const cm = cs.QueryInterface(Ci.nsICookieManager);

function setCookie(name, url) {
  let value = `${name}=${Math.random()}; Path=/; Max-Age=1000; sameSite=none`;
  info(`Setting cookie ${value} for ${url.spec}`);

  let channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  cs.setCookieStringFromHttp(url, value, channel);
}

async function sleep() {
  await new Promise(resolve => do_timeout(1000, resolve));
}

function checkSorting(cookies) {
  for (let i = 1; i < cookies.length; ++i) {
    Assert.greater(
      cookies[i].creationTime,
      cookies[i - 1].creationTime,
      "Cookie " + cookies[i].name
    );
  }
}

add_task(async function() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  await setCookie("A", Services.io.newURI("https://example.com/A/"));
  await sleep();

  await setCookie("B", Services.io.newURI("https://foo.bar/B/"));
  await sleep();

  await setCookie("C", Services.io.newURI("https://example.org/C/"));
  await sleep();

  await setCookie("D", Services.io.newURI("https://example.com/D/"));
  await sleep();

  Assert.equal(cm.cookies.length, 4, "Cookie check");

  const cookies = cm.getCookiesSince(0);
  Assert.equal(cookies.length, 4, "We retrieve all the 4 cookies");
  checkSorting(cookies);

  let someCookies = cm.getCookiesSince(cookies[0].creationTime + 1);
  Assert.equal(someCookies.length, 3, "We retrieve some cookies");
  checkSorting(someCookies);

  someCookies = cm.getCookiesSince(cookies[1].creationTime + 1);
  Assert.equal(someCookies.length, 2, "We retrieve some cookies");
  checkSorting(someCookies);

  someCookies = cm.getCookiesSince(cookies[2].creationTime + 1);
  Assert.equal(someCookies.length, 1, "We retrieve some cookies");
  checkSorting(someCookies);

  someCookies = cm.getCookiesSince(cookies[3].creationTime + 1);
  Assert.equal(someCookies.length, 0, "We retrieve some cookies");
});
