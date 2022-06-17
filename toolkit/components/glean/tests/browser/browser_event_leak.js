/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  Assert.equal(
    undefined,
    Glean.testOnlyIpc.eventWithExtra.testGetValue(),
    "Nothing to begin with"
  );
  Glean.testOnlyIpc.eventWithExtra.record({
    extra1: "Some extra string",
    extra2: 42,
    extra3_longer_name: false,
  });
  Assert.equal(
    1,
    Glean.testOnlyIpc.eventWithExtra.testGetValue().length,
    "One event? One event."
  );

  // AND NOW, FOR THE TRUE TEST:
  // Will this leak memory all over the place?
});
