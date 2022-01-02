"use strict";

CookiePolicyHelper.runTest("document.cookies", {
  cookieJarAccessAllowed: async _ => {
    let hasCookie = !!content.document.cookie.length;

    await content
      .fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          hasCookie ? "cookie-present" : "cookie-not-present",
          "document.cookie is consistent with fetch requests"
        );
      });

    content.document.cookie = "name=value";
    ok(content.document.cookie.includes("name=value"), "Some cookies for me");
    ok(content.document.cookie.includes("foopy=1"), "Some cookies for me");

    await content
      .fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-present", "We should have cookies");
      });

    ok(!!content.document.cookie.length, "Some Cookies for me");
  },

  cookieJarAccessDenied: async _ => {
    is(content.document.cookie, "", "No cookies for me");
    content.document.cookie = "name=value";
    is(content.document.cookie, "", "No cookies for me");

    await content
      .fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have cookies");
      });
    // Let's do it twice.
    await content
      .fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have cookies");
      });

    is(content.document.cookie, "", "Still no cookies for me");
  },
});
