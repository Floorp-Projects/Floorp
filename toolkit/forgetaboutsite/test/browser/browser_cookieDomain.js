let {ForgetAboutSite} = ChromeUtils.import("resource://gre/modules/ForgetAboutSite.jsm");

function checkCookie(host, originAttributes) {
  for (let cookie of Services.cookies.enumerator) {
    if (ChromeUtils.isOriginAttributesEqual(originAttributes,
                                            cookie.originAttributes) &&
        cookie.host.includes(host)) {
      return true;
    }
  }
  return false;
}

add_task(async _ => {
  info("Test single cookie domain");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  // A cookie domain
  Services.cookies.add(".example.com", "/test", "foo", "bar",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);

  // Cleaning up.
  await ForgetAboutSite.removeDataFromDomain("example.com");

  // All good.
  ok(!checkCookie("example.com", {}), "No cookies");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);
});
