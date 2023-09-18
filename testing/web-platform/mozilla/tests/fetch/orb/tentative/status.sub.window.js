// META: script=/fetch/orb/resources/utils.js
// META: script=resources/utils.js

const path = "http://{{domains[www1]}}:{{ports[http][0]}}/fetch/orb/resources";

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(
    `${path}/data.json`,
    null,
    contentType("application/json"),
    "status(206)"
  ),
  "ORB should filter opaque-response-blocklisted MIME type with status 206"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(
    `${path}/data.json`,
    null,
    contentType("application/json"),
    "status(302)"
  ),
  "ORB should filter opaque range of image/png not starting at zero, that isn't subsequent"
);
