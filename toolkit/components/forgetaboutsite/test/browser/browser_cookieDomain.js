const { ForgetAboutSite } = ChromeUtils.importESModule(
  "resource://gre/modules/ForgetAboutSite.sys.mjs"
);
const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

function checkCookie(host, originAttributes) {
  for (let cookie of Services.cookies.cookies) {
    if (
      ChromeUtils.isOriginAttributesEqual(
        originAttributes,
        cookie.originAttributes
      ) &&
      cookie.host.includes(host)
    ) {
      return true;
    }
  }
  return false;
}

add_task(async function test_singleDomain() {
  info("Test single cookie domain");

  // Let's clean up all the data.
  await SiteDataTestUtils.clear();

  SiteDataTestUtils.addToCookies({ origin: "https://example.com" });

  // Cleaning up.
  await ForgetAboutSite.removeDataFromDomain("example.com");

  // All good.
  ok(!checkCookie("example.com", {}), "No cookies");

  // Clean up.
  await SiteDataTestUtils.clear();
});

add_task(async function test_subDomain() {
  info("Test cookies for sub domains");

  // Let's clean up all the data.
  await SiteDataTestUtils.clear();

  SiteDataTestUtils.addToCookies({ origin: "https://example.com" });
  SiteDataTestUtils.addToCookies({ origin: "https://sub.example.com" });
  SiteDataTestUtils.addToCookies({ origin: "https://sub2.example.com" });
  SiteDataTestUtils.addToCookies({ origin: "https://sub2.example.com" });

  SiteDataTestUtils.addToCookies({ origin: "https://example.org" });

  // Cleaning up.
  await ForgetAboutSite.removeDataFromDomain("sub.example.com");

  // All good.
  ok(!checkCookie("example.com", {}), "No cookies for example.com");
  ok(checkCookie("example.org", {}), "Has cookies for example.org");

  // Clean up.
  await SiteDataTestUtils.clear();
});
