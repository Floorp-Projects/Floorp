/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm", {});

add_task(async function() {
  requestLongerTimeout(2);
  ok(true, "Collecting baseline coverage for javascript (.js) file types.");
  await new Promise((c) => setTimeout(c, 30 * 1000));
});
