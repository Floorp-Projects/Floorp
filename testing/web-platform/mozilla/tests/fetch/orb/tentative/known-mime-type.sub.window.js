// META: script=/fetch/orb/resources/utils.js
// META: script=resources/utils.js

const path = "http://{{domains[www1]}}:{{ports[http][0]}}/fetch/orb/resources";

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(`${path}/font.ttf`, null, contentType("font/ttf")),
  "ORB should filter opaque font/ttf"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(`${path}/text.txt`, null, contentType("text/plain")),
  "ORB should filter opaque text/plain"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(`${path}/data.json`, null, contentType("application/json")),
  "ORB should filter opaque application/json (non-empty)"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(`${path}/empty.json`, null, contentType("application/json")),
  "ORB should filter opaque application/json (empty)"
);

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(
    `${path}/data_non_ascii.json`,
    null,
    contentType("application/json")
  ),
  "ORB should filter opaque application/json which contains non ascii characters"
);
