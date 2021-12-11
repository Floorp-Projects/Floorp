/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function fetchDomainDisabled({ client }) {
  const { Fetch } = client;

  await Fetch.disable();
  ok("Disabling Fetch domain successful");
});
