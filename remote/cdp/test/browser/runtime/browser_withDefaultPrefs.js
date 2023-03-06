/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function enableRuntime_noHangAfterNavigation({ client }) {
  await loadURL(PAGE_URL);
  await enableRuntime(client);
});
