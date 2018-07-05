/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure the moz_origins table and origin frecency stats are updated
// correctly.

"use strict";

// Visiting a URL with a new origin should immediately update moz_origins.
add_task(async function visit() {
  await checkDB([]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([]);
  await cleanUp();
});


// Repeatedly visiting a URL with an initially new origin should update
// moz_origins (with the correct frecency).
add_task(async function visitRepeatedly() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com/" },
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([]);
  await cleanUp();
});


// Same as previous, but visits are added sequentially.
add_task(async function visitRepeatedlySequential() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([]);
  await cleanUp();
});


// After removing an origin's URLs, visiting a URL with the origin should
// immediately update moz_origins.
add_task(async function vistAfterDelete() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([]);
  await cleanUp();
});


// Visiting different URLs with the same origin should update moz_origins, and
// moz_origins.frecency should be the sum of the URL frecencies.
add_task(async function visitDifferentURLsSameOrigin() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/1" },
    { uri: "http://example.com/2" },
    { uri: "http://example.com/3" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/1",
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/1");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/2");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/3");
  await checkDB([]);
  await cleanUp();
});


// Same as previous, but visits are added sequentially.
add_task(async function visitDifferentURLsSameOriginSequential() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/1" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/1",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/2" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/1",
      "http://example.com/2",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/3" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/1",
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/1");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/2");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/3");
  await checkDB([]);
  await cleanUp();
});


// Repeatedly visiting different URLs with the same origin should update
// moz_origins (with the correct frecencies), and moz_origins.frecency should be
// the sum of the URL frecencies.
add_task(async function visitDifferentURLsSameOriginRepeatedly() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/1" },
    { uri: "http://example.com/1" },
    { uri: "http://example.com/1" },
    { uri: "http://example.com/2" },
    { uri: "http://example.com/2" },
    { uri: "http://example.com/3" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/1",
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/1");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/2",
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/2");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/3",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/3");
  await checkDB([]);
  await cleanUp();
});


// Visiting URLs with different origins should update moz_origins.
add_task(async function visitDifferentOrigins() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/" },
    { uri: "http://example2.com/" },
    { uri: "http://example3.com/" },
  ]);
  await checkDB([
    ["http://", "example1.com", ["http://example1.com/"]],
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/");
  await checkDB([
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/");
  await checkDB([
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/");
  await checkDB([]);
  await cleanUp();
});


// Same as previous, but visits are added sequentially.
add_task(async function visitDifferentOriginsSequential() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/" },
  ]);
  await checkDB([
    ["http://", "example1.com", ["http://example1.com/"]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example2.com/" },
  ]);
  await checkDB([
    ["http://", "example1.com", ["http://example1.com/"]],
    ["http://", "example2.com", ["http://example2.com/"]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example3.com/" },
  ]);
  await checkDB([
    ["http://", "example1.com", ["http://example1.com/"]],
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/");
  await checkDB([
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/");
  await checkDB([
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/");
  await checkDB([]);
  await cleanUp();
});


// Repeatedly visiting URLs with different origins should update moz_origins
// (with the correct frecencies).
add_task(async function visitDifferentOriginsRepeatedly() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/" },
    { uri: "http://example1.com/" },
    { uri: "http://example1.com/" },
    { uri: "http://example2.com/" },
    { uri: "http://example2.com/" },
    { uri: "http://example3.com/" },
  ]);
  await checkDB([
    ["http://", "example1.com", ["http://example1.com/"]],
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/");
  await checkDB([
    ["http://", "example2.com", ["http://example2.com/"]],
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/");
  await checkDB([
    ["http://", "example3.com", ["http://example3.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/");
  await checkDB([]);
  await cleanUp();
});


// Visiting URLs, some with the same and some with different origins, should
// update moz_origins.
add_task(async function visitDifferentOriginsDifferentURLs() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/1" },
    { uri: "http://example1.com/2" },
    { uri: "http://example1.com/3" },
    { uri: "http://example2.com/1" },
    { uri: "http://example2.com/2" },
    { uri: "http://example3.com/1" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/1");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/2");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/3");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/1");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/2");
  await checkDB([
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/1");
  await checkDB([]);
});


// Same as previous, but visits are added sequentially.
add_task(async function visitDifferentOriginsDifferentURLsSequential() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/1" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/2" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/3" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example2.com/1" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example2.com/2" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
  ]);
  await PlacesTestUtils.addVisits([
    { uri: "http://example3.com/1" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/1");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/2");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/3");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/1");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/2");
  await checkDB([
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/1");
  await checkDB([]);
});


// Repeatedly visiting URLs, some with the same and some with different origins,
// should update moz_origins (with the correct frecencies).
add_task(async function visitDifferentOriginsDifferentURLsRepeatedly() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example1.com/1" },
    { uri: "http://example1.com/1" },
    { uri: "http://example1.com/1" },
    { uri: "http://example1.com/2" },
    { uri: "http://example1.com/2" },
    { uri: "http://example1.com/3" },
    { uri: "http://example2.com/1" },
    { uri: "http://example2.com/1" },
    { uri: "http://example2.com/1" },
    { uri: "http://example2.com/2" },
    { uri: "http://example2.com/2" },
    { uri: "http://example3.com/1" },
    { uri: "http://example3.com/1" },
  ]);
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/1",
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/1");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/2",
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/2");
  await checkDB([
    ["http://", "example1.com", [
      "http://example1.com/3",
    ]],
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example1.com/3");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/1",
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/1");
  await checkDB([
    ["http://", "example2.com", [
      "http://example2.com/2",
    ]],
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example2.com/2");
  await checkDB([
    ["http://", "example3.com", [
      "http://example3.com/1",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example3.com/1");
  await checkDB([]);
});


// Makes sure URIs with the same TLD but different www subdomains are recognized
// as different origins.  Makes sure removing one doesn't remove the others.
add_task(async function www1() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "www.example.com", ["http://www.example.com/"]],
    ["http://", "www.www.example.com", ["http://www.www.example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "www.example.com", ["http://www.example.com/"]],
    ["http://", "www.www.example.com", ["http://www.www.example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "www.www.example.com", ["http://www.www.example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Same as www1, but removes URIs in a different order.
add_task(async function www2() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "www.example.com", ["http://www.example.com/"]],
    ["http://", "www.www.example.com", ["http://www.www.example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "www.example.com", ["http://www.example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure removing an origin without a port doesn't remove the same host
// with a port.
add_task(async function ports1() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com:8888/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "example.com:8888", ["http://example.com:8888/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "example.com:8888", ["http://example.com:8888/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure removing an origin with a port doesn't remove the same host
// without a port.
add_task(async function ports2() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com:8888/" },
  ]);
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "example.com:8888", ["http://example.com:8888/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure multiple URIs with the same origin don't create duplicate origins.
add_task(async function duplicates() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
    { uri: "https://example.com/" },
    { uri: "ftp://example.com/" },
    { uri: "foo://example.com/" },
    { uri: "bar:example.com/" },
    { uri: "http://example.com:8888/" },

    { uri: "http://example.com/dupe" },
    { uri: "http://www.example.com/dupe" },
    { uri: "http://www.www.example.com/dupe" },
    { uri: "https://example.com/dupe" },
    { uri: "ftp://example.com/dupe" },
    { uri: "foo://example.com/dupe" },
    { uri: "bar:example.com/dupe" },
    { uri: "http://example.com:8888/dupe" },
  ]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/",
      "http://example.com/dupe",
    ]],
    ["http://", "www.example.com", [
      "http://www.example.com/",
      "http://www.example.com/dupe",
    ]],
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/",
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/dupe",
    ]],
    ["http://", "www.example.com", [
      "http://www.example.com/",
      "http://www.example.com/dupe",
    ]],
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/",
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com/dupe");
  await checkDB([
    ["http://", "www.example.com", [
      "http://www.example.com/",
      "http://www.example.com/dupe",
    ]],
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/",
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "www.example.com", [
      "http://www.example.com/dupe",
    ]],
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/",
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/dupe");
  await checkDB([
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/",
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
    ["http://", "www.www.example.com", [
      "http://www.www.example.com/dupe",
    ]],
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/dupe");
  await checkDB([
    ["https://", "example.com", [
      "https://example.com/",
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("https://example.com/");
  await checkDB([
    ["https://", "example.com", [
      "https://example.com/dupe",
    ]],
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("https://example.com/dupe");
  await checkDB([
    ["ftp://", "example.com", [
      "ftp://example.com/",
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("ftp://example.com/");
  await checkDB([
    ["ftp://", "example.com", [
      "ftp://example.com/dupe",
    ]],
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("ftp://example.com/dupe");
  await checkDB([
    ["foo://", "example.com", [
      "foo://example.com/",
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("foo://example.com/");
  await checkDB([
    ["foo://", "example.com", [
      "foo://example.com/dupe",
    ]],
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("foo://example.com/dupe");
  await checkDB([
    ["bar:", "example.com", [
      "bar:example.com/",
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("bar:example.com/");
  await checkDB([
    ["bar:", "example.com", [
      "bar:example.com/dupe",
    ]],
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("bar:example.com/dupe");
  await checkDB([
    ["http://", "example.com:8888", [
      "http://example.com:8888/",
      "http://example.com:8888/dupe",
    ]],
  ]);

  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
    ["http://", "example.com:8888", [
      "http://example.com:8888/dupe",
    ]],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/dupe");
  await checkDB([
  ]);

  await cleanUp();
});


// Makes sure adding and removing bookmarks creates origins.
add_task(async function addRemoveBookmarks() {
  let bookmarks = [];
  let urls = [
    "http://example.com/",
    "http://www.example.com/",
  ];
  for (let url of urls) {
    bookmarks.push(await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    }));
  }
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "www.example.com", ["http://www.example.com/"]],
  ]);
  await PlacesUtils.bookmarks.remove(bookmarks[0]);
  await PlacesUtils.history.clear();
  await checkDB([
    ["http://", "www.example.com", ["http://www.example.com/"]],
  ]);
  await PlacesUtils.bookmarks.remove(bookmarks[1]);
  await PlacesUtils.history.clear();
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure changing bookmarks also changes the corresponding origins.
add_task(async function changeBookmarks() {
  let bookmarks = [];
  let urls = [
    "http://example.com/",
    "http://www.example.com/",
  ];
  for (let url of urls) {
    bookmarks.push(await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    }));
  }
  await checkDB([
    ["http://", "example.com", ["http://example.com/"]],
    ["http://", "www.example.com", ["http://www.example.com/"]],
  ]);
  await PlacesUtils.bookmarks.update({
    url: "http://www.example.com/",
    guid: bookmarks[0].guid,
  });
  await PlacesUtils.history.clear();
  await checkDB([
    ["http://", "www.example.com", ["http://www.example.com/"]],
  ]);
  await cleanUp();
});


// A slightly more complex test to make sure origin frecency stats are updated
// when visits and bookmarks are added and removed.
add_task(async function moreOriginFrecencyStats() {
  await checkDB([]);

  // Add a URL 0 visit.
  await PlacesTestUtils.addVisits([{ uri: "http://example.com/0" }]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
    ]],
  ]);

  // Add a URL 1 visit.
  await PlacesTestUtils.addVisits([{ uri: "http://example.com/1" }]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
    ]],
  ]);

  // Add a URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: "http://example.com/2" }]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
      "http://example.com/2",
    ]],
  ]);

  // Add another URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: "http://example.com/2" }]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
      "http://example.com/2",
    ]],
  ]);

  // Remove URL 2's visits.
  await PlacesUtils.history.remove(["http://example.com/2"]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
    ]],
  ]);

  // Bookmark URL 1.
  let parentGuid =
    await PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid,
    title: "A bookmark",
    url: NetUtil.newURI("http://example.com/1"),
  });
  await PlacesUtils.promiseItemId(bookmark.guid);

  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
    ]],
  ]);

  // Remove URL 1's visit.
  await PlacesUtils.history.remove(["http://example.com/1"]);
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
      "http://example.com/1",
    ]],
  ]);

  // Remove URL 1's bookmark.  Also need to call history.remove() again to
  // remove the URL from moz_places.  Otherwise it sticks around and keeps
  // contributing to the frecency stats.
  await PlacesUtils.bookmarks.remove(bookmark);
  await PlacesUtils.history.remove("http://example.com/1");
  await checkDB([
    ["http://", "example.com", [
      "http://example.com/0",
    ]],
  ]);

  // Remove URL 0.
  await PlacesUtils.history.remove(["http://example.com/0"]);
  await checkDB([
  ]);

  await cleanUp();
});


/**
 * Returns the expected frecency of the origin of the given URLs, i.e., the sum
 * of their frecencies.  Each URL is expected to have the same origin.
 *
 * @param  urls
 *         An array of URL strings.
 * @return The expected origin frecency.
 */
function expectedOriginFrecency(urls) {
  return urls.reduce((sum, url) => sum + Math.max(frecencyForUrl(url), 0), 0);
}

/**
 * Asserts that the moz_origins table and the origin frecency stats are correct.
 *
 * @param expectedOrigins
 *        An array of expected origins.  Each origin in the array is itself an
 *        array that looks like this:
 *          [prefix, host, [url1, url2, ..., urln]]
 *        The element at index 2 is an array of all the URLs with the origin.
 *        If you don't care about checking frecencies and origin frecency stats,
 *        this element can be `undefined`.
 */
async function checkDB(expectedOrigins) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT prefix, host, frecency
    FROM moz_origins
    ORDER BY id ASC
  `);
  let checkFrecencies = !expectedOrigins.length || expectedOrigins[0][2] !== undefined;
  let actualOrigins = rows.map(row => {
    let o = [];
    for (let c = 0; c < (checkFrecencies ? 3 : 2); c++) {
      o.push(row.getResultByIndex(c));
    }
    return o;
  });
  expectedOrigins = expectedOrigins.map(o => {
    return o.slice(0, 2).concat(
      checkFrecencies ? expectedOriginFrecency(o[2]) : []
    );
  });
  Assert.deepEqual(actualOrigins, expectedOrigins);
  if (checkFrecencies) {
    await checkStats(expectedOrigins.map(o => o[2]).filter(o => o > 0));
  }
}

/**
 * Asserts that the origin frecency stats are correct.
 *
 * @param expectedOriginFrecencies
 *        An array of expected origin frecencies.
 */
async function checkStats(expectedOriginFrecencies) {
  let stats = await promiseStats();
  Assert.equal(
    stats.count,
    expectedOriginFrecencies.length
  );
  Assert.equal(
    stats.sum,
    expectedOriginFrecencies.reduce((sum, f) => sum + f, 0)
  );
  Assert.equal(
    stats.squares,
    expectedOriginFrecencies.reduce((squares, f) => squares + (f * f), 0)
  );
}

/**
 * Returns the origin frecency stats.
 *
 * @return An object: { count, sum, squares }
 */
async function promiseStats() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_count"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_sum"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_sum_of_squares"), 0)
  `);
  return {
    count: rows[0].getResultByIndex(0),
    sum: rows[0].getResultByIndex(1),
    squares: rows[0].getResultByIndex(2),
  };
}

async function cleanUp() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
