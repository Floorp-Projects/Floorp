/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const cps = Cc["@mozilla.org/addons/content-policy;1"].getService(
  Ci.nsIAddonContentPolicy
);

add_task(async function test_csp_validator_flags() {
  let checkPolicy = (policy, flags, expectedResult, message = null) => {
    info(`Checking policy: ${policy}`);

    let result = cps.validateAddonCSP(policy, flags);
    equal(result, expectedResult);
  };

  let flags = Ci.nsIAddonContentPolicy;

  checkPolicy(
    "default-src 'self'; script-src 'self' http://localhost",
    0,
    "\u2018script-src\u2019 directive contains a forbidden http: protocol source",
    "localhost disallowed"
  );
  checkPolicy(
    "default-src 'self'; script-src 'self' http://localhost",
    flags.CSP_ALLOW_LOCALHOST,
    null,
    "localhost allowed"
  );

  checkPolicy(
    "default-src 'self'; script-src 'self' 'unsafe-eval'",
    0,
    "\u2018script-src\u2019 directive contains a forbidden 'unsafe-eval' keyword",
    "eval disallowed"
  );
  checkPolicy(
    "default-src 'self'; script-src 'self' 'unsafe-eval'",
    flags.CSP_ALLOW_EVAL,
    null,
    "eval allowed"
  );

  checkPolicy(
    "default-src 'self'; script-src 'self' https://example.com",
    0,
    "\u2018script-src\u2019 directive contains a forbidden https: protocol source",
    "remote disallowed"
  );
  checkPolicy(
    "default-src 'self'; script-src 'self' https://example.com",
    flags.CSP_ALLOW_REMOTE,
    null,
    "remote allowed"
  );
});

add_task(async function test_csp_validator() {
  let checkPolicy = (policy, expectedResult, message = null) => {
    info(`Checking policy: ${policy}`);

    let result = cps.validateAddonCSP(
      policy,
      Ci.nsIAddonContentPolicy.CSP_ALLOW_ANY
    );
    equal(result, expectedResult);
  };

  checkPolicy("script-src 'self'; object-src 'self';", null);

  let hash =
    "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  checkPolicy(
    `script-src 'self' https://com https://*.example.com moz-extension://09abcdef blob: filesystem: ${hash} 'unsafe-eval'; ` +
      `object-src 'self' https://com https://*.example.com moz-extension://09abcdef blob: filesystem: ${hash}`,
    null
  );

  checkPolicy(
    "",
    "Policy is missing a required \u2018script-src\u2019 directive"
  );

  checkPolicy(
    "object-src 'none';",
    "Policy is missing a required \u2018script-src\u2019 directive"
  );

  checkPolicy(
    "default-src 'self'",
    null,
    "A valid default-src should count as a valid script-src or object-src"
  );

  checkPolicy(
    "default-src 'self'; script-src 'self'",
    null,
    "A valid default-src should count as a valid script-src or object-src"
  );

  checkPolicy(
    "default-src 'self'; object-src 'self'",
    null,
    "A valid default-src should count as a valid script-src or object-src"
  );

  checkPolicy(
    "default-src 'self'; script-src http://example.com",
    "\u2018script-src\u2019 directive contains a forbidden http: protocol source",
    "A valid default-src should not allow an invalid script-src directive"
  );

  checkPolicy(
    "default-src 'self'; object-src http://example.com",
    "\u2018object-src\u2019 directive contains a forbidden http: protocol source",
    "A valid default-src should not allow an invalid object-src directive"
  );

  checkPolicy(
    "script-src 'self';",
    "Policy is missing a required \u2018object-src\u2019 directive"
  );

  checkPolicy(
    "script-src 'none'; object-src 'none'",
    "\u2018script-src\u2019 must include the source 'self'"
  );

  checkPolicy("script-src 'self'; object-src 'none';", null);

  checkPolicy(
    "script-src 'self' 'unsafe-inline'; object-src 'self';",
    "\u2018script-src\u2019 directive contains a forbidden 'unsafe-inline' keyword"
  );

  // Localhost is always valid
  for (let src of [
    "http://localhost",
    "https://localhost",
    "http://127.0.0.1",
    "https://127.0.0.1",
  ]) {
    checkPolicy(`script-src 'self' ${src}; object-src 'none';`, null);
  }

  let directives = ["script-src", "object-src"];

  for (let [directive, other] of [directives, directives.slice().reverse()]) {
    for (let src of ["https://*", "https://*.blogspot.com", "https://*"]) {
      checkPolicy(
        `${directive} 'self' ${src}; ${other} 'self';`,
        `https: wildcard sources in \u2018${directive}\u2019 directives must include at least one non-generic sub-domain (e.g., *.example.com rather than *.com)`
      );
    }

    for (let protocol of ["http", "https"]) {
      checkPolicy(
        `${directive} 'self' ${protocol}:; ${other} 'self';`,
        `${protocol}: protocol requires a host in \u2018${directive}\u2019 directives`
      );
    }

    checkPolicy(
      `${directive} 'self' http://example.com; ${other} 'self';`,
      `\u2018${directive}\u2019 directive contains a forbidden http: protocol source`
    );

    for (let protocol of ["ftp", "meh"]) {
      checkPolicy(
        `${directive} 'self' ${protocol}:; ${other} 'self';`,
        `\u2018${directive}\u2019 directive contains a forbidden ${protocol}: protocol source`
      );
    }

    checkPolicy(
      `${directive} 'self' 'nonce-01234'; ${other} 'self';`,
      `\u2018${directive}\u2019 directive contains a forbidden 'nonce-*' keyword`
    );
  }
});

add_task(async function test_csp_validator_extension_pages() {
  let checkPolicy = (policy, expectedResult, message = null) => {
    info(`Checking policy: ${policy}`);

    let result = cps.validateAddonCSP(
      policy,
      Ci.nsIAddonContentPolicy.CSP_ALLOW_LOCALHOST
    );
    equal(result, expectedResult);
  };

  checkPolicy("script-src 'self'; object-src 'self';", null);
  checkPolicy("script-src 'self'; object-src 'self'; worker-src 'none'", null);
  checkPolicy("script-src 'self'; object-src 'none'; worker-src 'self'", null);

  let hash =
    "'sha256-NjZhMDQ1YjQ1MjEwMmM1OWQ4NDBlYzA5N2Q1OWQ5NDY3ZTEzYTNmMzRmNjQ5NGU1MzlmZmQzMmMxYmIzNWYxOCAgLQo='";

  checkPolicy(
    `script-src 'self' moz-extension://09abcdef blob: filesystem: ${hash}; ` +
      `object-src 'self' moz-extension://09abcdef blob: filesystem: ${hash}`,
    null
  );

  for (let policy of ["", "object-src 'none';", "worker-src 'none';"]) {
    checkPolicy(
      policy,
      "Policy is missing a required \u2018script-src\u2019 directive"
    );
  }

  checkPolicy(
    "default-src 'self'",
    null,
    "A valid default-src should count as a valid script-src or object-src"
  );

  for (let directive of ["script-src", "object-src", "worker-src"]) {
    checkPolicy(
      `default-src 'self'; ${directive} 'self'`,
      null,
      `A valid default-src should count as a valid ${directive}`
    );
    checkPolicy(
      `default-src 'self'; ${directive} http://example.com`,
      `\u2018${directive}\u2019 directive contains a forbidden http: protocol source`,
      `A valid default-src should not allow an invalid ${directive} directive`
    );
  }

  checkPolicy(
    "script-src 'self';",
    "Policy is missing a required \u2018object-src\u2019 directive"
  );

  checkPolicy(
    "script-src 'none'; object-src 'none'",
    "\u2018script-src\u2019 must include the source 'self'"
  );

  checkPolicy("script-src 'self'; object-src 'none';", null);

  checkPolicy(
    "script-src 'self' 'unsafe-inline'; object-src 'self';",
    "\u2018script-src\u2019 directive contains a forbidden 'unsafe-inline' keyword"
  );

  checkPolicy(
    "script-src 'self' 'unsafe-eval'; object-src 'self';",
    "\u2018script-src\u2019 directive contains a forbidden 'unsafe-eval' keyword"
  );

  // Localhost is always valid
  for (let src of [
    "http://localhost",
    "https://localhost",
    "http://127.0.0.1",
    "https://127.0.0.1",
  ]) {
    checkPolicy(`script-src 'self' ${src}; object-src 'none';`, null);
  }

  let directives = ["script-src", "object-src"];

  for (let [directive, other] of [directives, directives.slice().reverse()]) {
    for (let protocol of ["http", "https"]) {
      checkPolicy(
        `${directive} 'self' ${protocol}:; ${other} 'self';`,
        `${protocol}: protocol requires a host in \u2018${directive}\u2019 directives`
      );
    }

    checkPolicy(
      `${directive} 'self' https://example.com; ${other} 'self';`,
      `\u2018${directive}\u2019 directive contains a forbidden https: protocol source`
    );

    checkPolicy(
      `${directive} 'self' http://example.com; ${other} 'self';`,
      `\u2018${directive}\u2019 directive contains a forbidden http: protocol source`
    );

    for (let protocol of ["ftp", "meh"]) {
      checkPolicy(
        `${directive} 'self' ${protocol}:; ${other} 'self';`,
        `\u2018${directive}\u2019 directive contains a forbidden ${protocol}: protocol source`
      );
    }

    checkPolicy(
      `${directive} 'self' 'nonce-01234'; ${other} 'self';`,
      `\u2018${directive}\u2019 directive contains a forbidden 'nonce-*' keyword`
    );
  }
});
