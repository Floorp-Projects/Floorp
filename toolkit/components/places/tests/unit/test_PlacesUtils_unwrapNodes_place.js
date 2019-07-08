/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that unwrapNodes properly filters out place: uris from text flavors.

add_task(function() {
  let tests = [
    // Single url.
    ["place:type=0&sort=1:", PlacesUtils.TYPE_X_MOZ_URL],
    // Multiple urls.
    [
      "place:type=0&sort=1:\nfirst\nplace:type=0&sort=1\nsecond",
      PlacesUtils.TYPE_X_MOZ_URL,
    ],
    // Url == title.
    ["place:type=0&sort=1:\nplace:type=0&sort=1", PlacesUtils.TYPE_X_MOZ_URL],
    // Malformed.
    [
      "place:type=0&sort=1:\nplace:type=0&sort=1\nmalformed",
      PlacesUtils.TYPE_X_MOZ_URL,
    ],
    // Single url.
    ["place:type=0&sort=1:", PlacesUtils.TYPE_UNICODE],
    // Multiple urls.
    ["place:type=0&sort=1:\nplace:type=0&sort=1", PlacesUtils.TYPE_UNICODE],
  ];
  for (let [blob, type] of tests) {
    Assert.deepEqual(
      PlacesUtils.unwrapNodes(blob, type),
      [],
      "No valid entries should be found"
    );
  }
});
