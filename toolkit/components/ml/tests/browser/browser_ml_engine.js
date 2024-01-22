/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_ml_engine_exists() {
  const response = await fetch("chrome://global/content/ml/MLEngine.html");
  ok(response.ok, "The ml engine can be fetched.");
});
