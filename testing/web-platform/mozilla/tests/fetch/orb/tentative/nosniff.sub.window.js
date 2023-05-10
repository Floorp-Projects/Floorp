// META: script=/fetch/orb/resources/utils.js
// META: script=resources/utils.js

const path = "http://{{domains[www1]}}:{{ports[http][0]}}/fetch/orb/resources";

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  fetchORB(
    `${path}/data.json`,
    null,
    contentType("application/json"),
    contentTypeOptions("nosniff")
  ),
  "ORB should filter opaque-response-blocklisted MIME type with nosniff"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  fetchORB(
    `${path}/data.json`,
    null,
    contentType(""),
    contentTypeOptions("nosniff")
  ),
  "ORB should filter opaque response with empty Content-Type and nosniff"
);
