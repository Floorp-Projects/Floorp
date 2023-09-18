// META: script=/fetch/orb/resources/utils.js
// META: script=resources/utils.js

const url =
  "http://{{domains[www1]}}:{{ports[http][0]}}/fetch/orb/resources/image.png";

// Due to web compatibility we filter opaque Response object from the
// fetch() function in the Fetch specification. See Bug 1823877. This
// might be removed in the future.
promise_internal_response_is_filtered(
  testFetchNoCors(
    url,
    { headers: new Headers([["Range", "bytes 10-99"]]) },
    header("Content-Range", "bytes 10-99/1010"),
    "slice(10,100)",
    "status(206)"
  ),
  "ORB should filter opaque range of image/png not starting at zero, that isn't subsequent"
);
