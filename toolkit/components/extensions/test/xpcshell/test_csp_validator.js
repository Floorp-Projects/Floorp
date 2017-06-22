/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const cps = Cc["@mozilla.org/addons/content-policy;1"].getService(Ci.nsIAddonContentPolicy);

add_task(async function test_csp_validator() {
  let checkPolicy = (policy, expectedResult, message = null) => {
    do_print(`Checking policy: ${policy}`);

    let result = cps.validateAddonCSP(policy);
    equal(result, expectedResult);
  };

  checkPolicy("script-src 'self'; object-src 'self';",
              null);

  let hash = "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  checkPolicy(`script-src 'self' https://com https://*.example.com moz-extension://09abcdef blob: filesystem: ${hash} 'unsafe-eval'; ` +
              `object-src 'self' https://com https://*.example.com moz-extension://09abcdef blob: filesystem: ${hash}`,
              null);

  checkPolicy("",
              "Policy is missing a required \u2018script-src\u2019 directive");

  checkPolicy("object-src 'none';",
              "Policy is missing a required \u2018script-src\u2019 directive");


  checkPolicy("default-src 'self'", null,
              "A valid default-src should count as a valid script-src or object-src");

  checkPolicy("default-src 'self'; script-src 'self'", null,
              "A valid default-src should count as a valid script-src or object-src");

  checkPolicy("default-src 'self'; object-src 'self'", null,
              "A valid default-src should count as a valid script-src or object-src");


  checkPolicy("default-src 'self'; script-src http://example.com",
              "\u2018script-src\u2019 directive contains a forbidden http: protocol source",
              "A valid default-src should not allow an invalid script-src directive");

  checkPolicy("default-src 'self'; object-src http://example.com",
              "\u2018object-src\u2019 directive contains a forbidden http: protocol source",
              "A valid default-src should not allow an invalid object-src directive");


  checkPolicy("script-src 'self';",
              "Policy is missing a required \u2018object-src\u2019 directive");

  checkPolicy("script-src 'none'; object-src 'none'",
              "\u2018script-src\u2019 must include the source 'self'");

  checkPolicy("script-src 'self'; object-src 'none';",
              null);

  checkPolicy("script-src 'self' 'unsafe-inline'; object-src 'self';",
              "\u2018script-src\u2019 directive contains a forbidden 'unsafe-inline' keyword");


  let directives = ["script-src", "object-src"];

  for (let [directive, other] of [directives, directives.slice().reverse()]) {
    for (let src of ["https://*", "https://*.blogspot.com", "https://*"]) {
      checkPolicy(`${directive} 'self' ${src}; ${other} 'self';`,
                  `https: wildcard sources in \u2018${directive}\u2019 directives must include at least one non-generic sub-domain (e.g., *.example.com rather than *.com)`);
    }

    checkPolicy(`${directive} 'self' https:; ${other} 'self';`,
                `https: protocol requires a host in \u2018${directive}\u2019 directives`);

    checkPolicy(`${directive} 'self' http://example.com; ${other} 'self';`,
                `\u2018${directive}\u2019 directive contains a forbidden http: protocol source`);

    for (let protocol of ["http", "ftp", "meh"]) {
      checkPolicy(`${directive} 'self' ${protocol}:; ${other} 'self';`,
                  `\u2018${directive}\u2019 directive contains a forbidden ${protocol}: protocol source`);
    }

    checkPolicy(`${directive} 'self' 'nonce-01234'; ${other} 'self';`,
                `\u2018${directive}\u2019 directive contains a forbidden 'nonce-*' keyword`);
  }
});
